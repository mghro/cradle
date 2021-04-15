#include <cradle/background/job.h>

#include <cradle/background/internals.h>

namespace cradle {

background_job_controller::~background_job_controller()
{
}
void
background_job_controller::reset()
{
    job_.reset();
}
background_job_state
background_job_controller::state() const
{
    assert(job_);
    return job_->state.load(std::memory_order_relaxed);
}
optional<float>
background_job_controller::progress() const
{
    assert(job_);
    return decode_progress(job_->progress.load(std::memory_order_relaxed));
}
void
background_job_controller::cancel()
{
    if (job_)
        job_->cancel = true;
}

namespace detail {

// Add a background job to the given execution pool and take care of the
// mechanics for ensuring that a thread gets woken up to handle it.
//
// If :ensure_idle_thread_exists is true, this will ensure that an idle thread
// exists to pick up the job. Otherwise, the job might get queued until a
// thread is available.
//
template<class ExecutionLoop>
void
queue_background_job(
    background_execution_pool& pool,
    background_job_ptr job_ptr,
    bool ensure_idle_thread_exists = false)
{
    background_job_queue& queue = *pool.queue;
    {
        std::scoped_lock<std::mutex> lock(queue.mutex);
        ++queue.version;
        if (!(job_ptr->flags & BACKGROUND_JOB_HIDDEN))
        {
            queue.job_info[&*job_ptr]
                = background_job_info(); // TODO: job_ptr->job->get_info();
            ++queue.reported_size;
        }
        queue.jobs.push(job_ptr);
        // If requested, ensure that there will be an idle thread to pick up
        // the new job.
        if (ensure_idle_thread_exists
            && queue.n_idle_threads < queue.jobs.size())
        {
            add_background_thread<ExecutionLoop>(pool);
        }
    }
    queue.cv.notify_one();
}


// enum class background_job_queue_type
// {
//     // Calculation jobs are run in parallel according to the number of
//     // available process cores.
//     CALCULATION = 0,

//     // Disk jobs are run with a much lower level of parallelism since it's
//     // assumed that disk bandwidth is going to limit parallelism.
//     DISK,

//     // HTTP job are run with a very high level of parallelism.
//     // (It would be essentially infinite, but we've determined that this can
//     // cause issues with routers/ISPs getting upset when clients have too many
//     // open connections.)
//     HTTP,

//     // Jobs in the following queues are long-lived network request jobs that
//     // may run indefinitely but consume very little bandwidth, so they each get
//     // their own thread.
//     NOTIFICATION_WATCH,
//     REMOTE_CALCULATION,

//     // This is just here to capture the count of queue types.
//     COUNT
// };

// Queue a background job to the requested queue within the background
// execution system.
void
queue_background_job(
    background_execution_system& system,
    background_job_queue_type queue,
    background_job_ptr job_ptr)
{
    // switch (queue)
    // {
    //     case background_job_queue_type::CALCULATION:
    //         queue_background_job<background_job_execution_loop>(
    //             system.pools[int(queue)], std::move(job_ptr));
    //         break;
    //     case background_job_queue_type::DISK:
    //         queue_background_job<background_job_execution_loop>(
    //             system.pools[int(queue)], std::move(job_ptr));
    //         break;
    //     case background_job_queue_type::HTTP:
    //         queue_background_job<http_request_processing_loop>(
    //             system.pools[int(queue)], std::move(job_ptr));
    //         break;
    //     case background_job_queue_type::NOTIFICATION_WATCH:
    //         queue_background_job<http_request_processing_loop>(
    //             system.pools[int(queue)], std::move(job_ptr), true);
    //         break;
    //     case background_job_queue_type::REMOTE_CALCULATION:
    //         queue_background_job<http_request_processing_loop>(
    //             system.pools[int(queue)], std::move(job_ptr), true);
    //         break;
    //     case background_job_queue_type::COUNT: // To silence warnings.
    //         break;
    // }
}

// void static
// initialize_system(background_execution_system_impl& system)
// {
//     // Only enable full concurrency in release mode.
//     // I've had issues with running inside the debugger with too many threads,
//     // and it's just easier to see what's going on with less concurrency.
//     // (The app even feels faster in debug mode with fewer threads.)
//   #ifdef _DEBUG
//     bool const full_concurrency = false;
//   #else
//     bool const full_concurrency = true;
//   #endif
//     // Initialize all the queues.
//     initialize_pool<background_job_execution_loop>(
//         system.pools[int(background_job_queue_type::CALCULATION)],
//         full_concurrency ? boost::thread::hardware_concurrency() : 1);
//     initialize_pool<web_request_processing_loop>(
//         system.pools[int(background_job_queue_type::WEB_READ)],
//         full_concurrency ? 16 : 1);
//     initialize_pool<web_request_processing_loop>(
//         system.pools[int(background_job_queue_type::WEB_WRITE)], 1);
//     initialize_pool<web_request_processing_loop>(
//         system.pools[int(background_job_queue_type::NOTIFICATION_WATCH)], 1);
//     initialize_pool<web_request_processing_loop>(
//         system.pools[int(background_job_queue_type::REMOTE_CALCULATION)], 1);
//     initialize_pool<background_job_execution_loop>(
//         system.pools[int(background_job_queue_type::DISK)],
//         full_concurrency ? 2 : 1);
//     // Invalidate the session data.
//     system.authentication.status =
//         background_authentication_status(
//             background_authentication_state::NO_CREDENTIALS);
//     system.context.status =
//         background_context_request_status(
//             background_context_request_state::NO_REQUEST);
// }

} // namespace detail

background_job_controller
add_background_job(
    background_execution_system& system,
    background_job_queue_type queue,
    std::unique_ptr<background_job_interface> job,
    background_job_flag_set flags,
    int priority)
{
    auto ptr = std::make_shared<detail::background_job_execution_data>(
        std::move(job), flags, priority);
    detail::queue_background_job(*system.impl_, queue, ptr);
    return background_job_controller(ptr);
}

} // namespace cradle
