#include <cradle/thinknode/provider.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <cradle/thinknode/ipc.hpp>
#include <cradle/thinknode/messages.hpp>

namespace cradle {

uint8_t static const ipc_version = 1;

// Check if an error occurred, and if so, throw an appropriate exception.
static void
check_error(boost::system::error_code const& error)
{
    if (error)
    {
        CRADLE_THROW(
            thinknode_provider_error()
            << internal_error_message_info(error.message()));
    }
}

struct calc_provider
{
    boost::asio::io_service io_service;
    tcp::socket socket;

    calc_provider() : socket(io_service)
    {
    }
};

// Does the calc provider have an incoming message?
// (This is a non-blocking poll to check for incoming data on the socket.)
static bool
has_incoming_message(calc_provider& provider)
{
    // Start a reactor-style read operation by providing a null_buffer.
    provider.socket.async_receive(
        boost::asio::null_buffers(),
        [](boost::system::error_code const& error,
           std::size_t bytes_transferred) { check_error(error); });

    // Poll the I/O service and see if the handler runs.
    bool ready_to_read = provider.io_service.poll() != 0;

    // The I/O service needs to be reset when we do stuff like this.
    provider.io_service.reset();

    return ready_to_read;
}

// internal_message_queue is used to transmit messages from the thread that's
// executing the calculation to the IPC thread.
struct internal_message_queue
{
    std::queue<thinknode_provider_message> messages;
    boost::mutex mutex;
    // for signaling when new messages arrive in the queue
    boost::condition_variable cv;
};

// Post a message to an internal_message_queue.
static void
post_message(
    internal_message_queue& queue, thinknode_provider_message&& message)
{
    boost::mutex::scoped_lock lock(queue.mutex);
    queue.messages.emplace(message);
    queue.cv.notify_one();
}

struct provider_progress_reporter : progress_reporter_interface
{
    void
    operator()(float progress)
    {
        post_message(
            *queue,
            make_thinknode_provider_message_with_progress(
                make_thinknode_provider_progress_update(progress, "")));
    }
    internal_message_queue* queue;
};

// Perform a calculation. (This is executed in a dedicated thread.)
static void
perform_calculation(
    internal_message_queue& queue,
    provider_app_interface const& app,
    thinknode_supervisor_calculation_request const& request)
{
    try
    {
        null_check_in check_in;

        provider_progress_reporter reporter;
        reporter.queue = &queue;

        auto result = app.execute_function(
            check_in, reporter, request.name, request.args);

        post_message(
            queue, make_thinknode_provider_message_with_result(result));
    }
    catch (std::exception& e)
    {
        post_message(
            queue,
            make_thinknode_provider_message_with_failure(
                make_thinknode_provider_failure(
                    "none", // TODO: Implement a system of error codes.
                    e.what())));
    }
}

// Check the internal queue for messages and transmit (and remove) any that
// are found. If it sees a message that indicates that the calculation is
// finished, it returns true.
// Note that the queue's mutex must be locked before calling this.
static bool
transmit_queued_messages(tcp::socket& socket, internal_message_queue& queue)
{
    while (!queue.messages.empty())
    {
        auto const& message = queue.messages.front();
        write_message(socket, ipc_version, message);
        // If we see either of these, we must be done.
        if (get_tag(message) == thinknode_provider_message_tag::RESULT
            || get_tag(message) == thinknode_provider_message_tag::FAILURE)
        {
            return true;
        }
        queue.messages.pop();
    }
    return false;
}

static void
dispatch_and_monitor_calculation(
    calc_provider& provider,
    provider_app_interface const& app,
    thinknode_supervisor_calculation_request const& request)
{
    internal_message_queue queue;

    // Dispatch a thread to perform the calculation.
    boost::thread thread{[&]() { perform_calculation(queue, app, request); }};

    // Monitor for messages from the calculation thread or the supervisor.
    // Note that it's difficult to have this thread wait on messages from both
    // the calculation and the supervisor, so it properly waits on the former
    // and periodically checks the latter. This means that it will pass along
    // results and progress reports almost immediately, but it will be a bit
    // delayed in responding to pings.
    while (true)
    {
        // Forward along messages that come from the calculation thread.
        {
            boost::mutex::scoped_lock lock(queue.mutex);
            // First transmit any messages that got queued while we were in
            // other parts of the loop.
            // (We may have missed the signal for these.)
            if (transmit_queued_messages(provider.socket, queue))
                break;
            // Now spend some time waiting for more messages to arrive.
            if (queue.cv.timed_wait(lock, boost::posix_time::seconds(1)))
            {
                if (transmit_queued_messages(provider.socket, queue))
                    break;
            }
        }

        // Now check for incoming messages on the socket.
        while (has_incoming_message(provider))
        {
            auto message = read_message<thinknode_supervisor_message>(
                provider.socket, ipc_version);
            switch (get_tag(message))
            {
                case thinknode_supervisor_message_tag::FUNCTION:
                    // The supervisor should prevent this from happening.
                    assert(0);
                    break;
                case thinknode_supervisor_message_tag::PING:
                    write_message(
                        provider.socket,
                        ipc_version,
                        make_thinknode_provider_message_with_pong(
                            as_ping(message)));
                    break;
            }
        }
    }

    thread.join();
}

void
provide_calculations(
    int argc, char const* const* argv, provider_app_interface const& app)
{
    auto host = getenv("THINKNODE_HOST");
    auto port = std::stoi(getenv("THINKNODE_PORT"));
    auto pid = getenv("THINKNODE_PID");

    calc_provider provider;

    // Resolve the address of the supervisor.
    tcp::resolver resolver(provider.io_service);
    tcp::resolver::query query(tcp::v4(), host, std::to_string(port));
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

    // Connect to the supervisor and send the registration message.
    boost::asio::connect(provider.socket, endpoint_iterator);
    write_message(
        provider.socket,
        ipc_version,
        make_thinknode_provider_message_with_registration(
            make_thinknode_provider_registration(0, pid)));

    // Process messages from the supervisor.
    while (1)
    {
        auto message = read_message<thinknode_supervisor_message>(
            provider.socket, ipc_version);
        switch (get_tag(message))
        {
            case thinknode_supervisor_message_tag::FUNCTION:
                dispatch_and_monitor_calculation(
                    provider, app, as_function(message));
                break;
            case thinknode_supervisor_message_tag::PING:
                write_message(
                    provider.socket,
                    ipc_version,
                    make_thinknode_provider_message_with_pong(
                        as_ping(message)));
                break;
        }
    }
}

} // namespace cradle
