#include <cradle/thinknode/supervisor.hpp>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/encodings/json.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/ipc.hpp>
#include <cradle/thinknode/messages.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace cradle {

uint8_t static const ipc_version = 1;

// BEWARE: This is pasted in string form in many places below...
uint16_t static const the_port = 41079;

static string
extract_tag(thinknode_provider_image_info const& image)
{
    switch (get_tag(image))
    {
        case thinknode_provider_image_info_tag::TAG:
            return as_tag(image);
        case thinknode_provider_image_info_tag::DIGEST:
            return as_digest(image);
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_provider_image_info_tag")
                << enum_value_info(static_cast<int>(get_tag(image))));
    }
}

enum class docker_service_type
{
    WINDOWS,
    LINUX
};

static http_request
make_docker_request(
    docker_service_type service_type,
    http_request_method method,
    string path,
    http_header_list headers,
    http_body body = blob())
{
    switch (service_type)
    {
        case docker_service_type::WINDOWS:
            return make_http_request(
                method, "http://localhost:2375" + path, headers, body);
        case docker_service_type::LINUX:
            return http_request{
                method,
                "http://localhost" + path,
                headers,
                body,
                some(string("/var/run/docker.sock"))};
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("docker_service_type")
                << enum_value_info(static_cast<int>(service_type)));
    }
}

static docker_service_type
detect_docker(http_connection& connection)
{
    // Try Linux.
    try
    {
        auto query = make_docker_request(
            docker_service_type::LINUX,
            http_request_method::GET,
            "/v1.38/version",
            http_header_list());
        null_check_in check_in;
        null_progress_reporter reporter;
        connection.perform_request(check_in, reporter, query);

        return docker_service_type::LINUX;
    }
    catch (...)
    {
    }

    // If the Linux way didn't work, assume Windows (but check it).

    auto query = make_docker_request(
        docker_service_type::WINDOWS,
        http_request_method::GET,
        "/v1.38/version",
        http_header_list());
    null_check_in check_in;
    null_progress_reporter reporter;
    connection.perform_request(check_in, reporter, query);

    return docker_service_type::WINDOWS;
}

static void
pull_image(
    docker_service_type service_type,
    http_connection& connection,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image)
{
    auto query = make_docker_request(
        service_type,
        http_request_method::POST,
        "/v1.38/images/create?fromImage=registry-mgh.thinknode.com/" + account
            + "/" + app + "&tag=" + extract_tag(image),
        {{"X-Registry-Auth",
          "ewogICJ1c2VybmFtZSI6ICJtZ2gvZG9ja2VyLWJvdCIsCiAgInBhc3N3b3JkIjog"
          "Im5w"
          "dGM0cHYyIiwKICAic2VydmVyYWRkcmVzcyI6ICJodHRwczovL3JlZ2lzdHJ5LW1n"
          "aC50"
          "aGlua25vZGUuY29tIgp9"}});
    null_check_in check_in;
    null_progress_reporter reporter;
    connection.perform_request(check_in, reporter, query);
}

static string
spawn_provider(
    docker_service_type service_type,
    http_connection& connection,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image)
{
    null_check_in check_in;
    null_progress_reporter reporter;

    // Create the container.
    string id;
    {
        auto request = make_docker_request(
            service_type,
            http_request_method::POST,
            "/v1.38/containers/create",
            {{"Content-Type", "application/json"},
             {"X-Registry-Auth",
              "ewogICJ1c2VybmFtZSI6ICJtZ2gvZG9ja2VyLWJvdCIsCiAgInBhc3N3b3Jk"
              "Ijog"
              "Im5wdGM0cHYyIiwKICAic2VydmVyYWRkcmVzcyI6ICJodHRwczovL3JlZ2lz"
              "dHJ5"
              "LW1naC50aGlua25vZGUuY29tIgp9"}},
            value_to_json_blob(dynamic(
                {{"Image",
                  "registry-mgh.thinknode.com/" + account + "/" + app + "@"
                      + extract_tag(image)},
                 {"Env",
                  {service_type == docker_service_type::WINDOWS
                       ? "THINKNODE_HOST=host.docker.internal"
                       : "THINKNODE_HOST=localhost",
                   "THINKNODE_PORT=41079",
                   "THINKNODE_PID=the_pid_which_must_be_length_32_"}},
                 {"HostConfig", {{"NetworkMode", "host"}}}})));
        auto response
            = connection.perform_request(check_in, reporter, request);
        id = cast<string>(
            get_field(cast<dynamic_map>(parse_json_response(response)), "Id"));
    }

    // Start it.
    {
        auto request = make_docker_request(
            service_type,
            http_request_method::POST,
            "/v1.38/containers/" + id + "/start",
            http_header_list());
        connection.perform_request(check_in, reporter, request);
    }

    return id;
}

dynamic
supervise_thinknode_calculation(
    http_connection& connection,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image,
    string const& function_name,
    std::vector<dynamic> const& args)
{
    std::cout << "----------------------------------------\n";
    std::cout << "LOCAL CALC...\n";
    std::cout << function_name << "\n";

    auto service_type = detect_docker(connection);

    pull_image(service_type, connection, account, app, image);

    asio::io_service io_service;

    tcp::acceptor a(
        io_service,
        tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), the_port));

    tcp::socket socket(io_service);

    dynamic result;

    // Dispatch a thread to process messages.
    boost::thread message_thread{[&]() {
        std::cout << "----------------------------------------\n";
        std::cout << "accepting...\n";
        a.accept(socket);
        std::cout << "ACCEPTED!\n";

        // Process messages from the supervisor.
        while (1)
        {
            auto message = read_message<thinknode_provider_message>(
                socket, ipc_version);
            switch (get_tag(message))
            {
                case thinknode_provider_message_tag::REGISTRATION:
                    std::cout << "----------------------------------------\n";
                    std::cout << "REGISTRATION!!!\n";
                    std::cout << as_registration(message) << std::endl;
                    std::cout << "----------------------------------------\n";
                    std::cout << std::endl;
                    write_message(
                        socket,
                        ipc_version,
                        make_thinknode_supervisor_message_with_function(
                            make_thinknode_supervisor_calculation_request(
                                function_name, args)));
                    std::cout << "ARGUMENTS SENT!!!\n";
                    std::cout << "----------------------------------------\n";
                    break;
                case thinknode_provider_message_tag::RESULT:
                    std::cout << "----------------------------------------\n";
                    std::cout << "RESULT!!!\n";
                    std::cout << as_result(message) << std::endl;
                    std::cout << "----------------------------------------\n";
                    std::cout << std::endl;
                    result = as_result(message);
                    return;
                default:
                    break;
            }
        }
    }};

    // TODO: Synchronize for real.
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    auto provider
        = spawn_provider(service_type, connection, account, app, image);
    std::cout << "----------------------------------------\n";
    std::cout << "SPAWNED\n";
    std::cout << provider << std::endl;

    message_thread.join();

    std::cout << "----------------------------------------\n";
    std::cout << "JOINED!\n";

    return result;
}

} // namespace cradle
