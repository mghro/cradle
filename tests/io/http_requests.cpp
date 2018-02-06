#include <cradle/io/http_requests.hpp>

#include <boost/algorithm/string.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/core/testing.hpp>
#include <cradle/core/utilities.hpp>
#include <cradle/encodings/json.hpp>

#include "http_requests.hpp"

using namespace cradle;

file_path
find_testing_cacert_file()
{
    try
    {
        return file_path(get_environment_variable("CRADLE_DEPLOY_DIR")) / "cacert.pem";
    }
    catch (...)
    {
        return file_path("cacert.pem");
    }
}

http_request_system static
the_http_request_system(some(find_testing_cacert_file()));

http_response
perform_simple_request(http_request const& request)
{
    http_connection connection(the_http_request_system);
    null_check_in check_in;
    null_progress_reporter reporter;
    return connection.perform_request(check_in, reporter, request);
}

TEST_CASE("request headers", "[io][http]")
{
    http_header_list request_headers =
        {
            { "Accept", "application/json" },
            { "Cradle-Test-Header", "present" },
            { "Color", "navy" }
        };
    auto response =
        perform_simple_request(
            make_get_request("http://postman-echo.com/headers", request_headers));
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    http_header_list response_headers;
    from_dynamic(&response_headers, get_field(cast<dynamic_map>(body), "headers"));
    for (auto const& header : request_headers)
    {
        CAPTURE(header.first);
        REQUIRE(
            response_headers.at(boost::algorithm::to_lower_copy(header.first)) == header.second);
    }
}

TEST_CASE("response headers", "[io][http]")
{
    auto response =
        perform_simple_request(
            make_get_request(
                "http://postman-echo.com/response-headers?Cradle-Test-Header=present&Color=navy",
                { { "Accept", "application/json" } }));
    REQUIRE(response.status_code == 200);
    http_header_list expected_headers =
        {
            { "Cradle-Test-Header", "present" },
            { "Color", "navy" }
        };
    for (auto const& header : expected_headers)
    {
        CAPTURE(header.first);
        REQUIRE(response.headers.at(header.first) == header.second);
    }
}

TEST_CASE("GET request", "[io][http]")
{
    auto response =
        perform_simple_request(
            make_get_request("http://postman-echo.com/get?color=navy", http_header_list()));
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(get_field(cast<dynamic_map>(body), "args") == dynamic({ { "color", "navy" } }));
}

TEST_CASE("HTTPS request", "[io][http]")
{
    auto response =
        perform_simple_request(
            make_get_request("https://postman-echo.com/get?color=navy", http_header_list()));
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(get_field(cast<dynamic_map>(body), "args") == dynamic({ { "color", "navy" } }));
}

void
test_method_with_content(http_request_method method)
{
    auto content =
        dynamic({ { "numbers", dynamic({ integer(4), integer(3), integer(2), integer(1) }) } });
    auto response =
        perform_simple_request(
            make_http_request(
                method,
                "http://postman-echo.com/" + string(get_value_id(method)),
                {
                    { "Accept", "application/json" },
                    { "Content-Type", "application/json" }
                },
                make_string_blob(value_to_json(content))));
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(get_field(cast<dynamic_map>(body), "json") == content);
}

TEST_CASE("PUT request", "[io][http]")
{
    test_method_with_content(http_request_method::PUT);
}

TEST_CASE("POST request", "[io][http]")
{
    test_method_with_content(http_request_method::POST);
}

TEST_CASE("DELETE request", "[io][http]")
{
    auto response =
        perform_simple_request(
            make_http_request(
                http_request_method::DELETE,
                "http://postman-echo.com/delete",
                http_header_list(),
                http_body()));
    REQUIRE(response.status_code == 200);
}

TEST_CASE("large HTTP request", "[io][http]")
{
    std::vector<integer> numbers;
    for (integer i = 0; i != 4096; ++i)
    {
        numbers.push_back(i);
    }
    auto content = dynamic({ { "numbers", to_dynamic(numbers) } });
    auto response =
        perform_simple_request(
            make_http_request(
                http_request_method::POST,
                "http://postman-echo.com/post",
                {
                    { "Accept", "application/json" },
                    { "Content-Type", "application/json" }
                },
                make_string_blob(value_to_json(content))));
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(get_field(cast<dynamic_map>(body), "json") == content);
}

TEST_CASE("404 response code", "[io][http]")
{
    auto request = make_get_request("http://postman-echo.com/status/404", http_header_list());
    try
    {
        perform_simple_request(request);
        FAIL("no exception thrown");
    }
    catch (bad_http_status_code& e)
    {
        REQUIRE(get_required_error_info<attempted_http_request_info>(e) == request);
        REQUIRE(get_required_error_info<http_response_info>(e).status_code == 404);
    }
}

TEST_CASE("500 response code", "[io][http]")
{
    auto request = make_get_request("http://postman-echo.com/status/500", http_header_list());
    try
    {
        perform_simple_request(request);
        FAIL("no exception thrown");
    }
    catch (bad_http_status_code& e)
    {
        REQUIRE(get_required_error_info<attempted_http_request_info>(e) == request);
        REQUIRE(get_required_error_info<http_response_info>(e).status_code == 500);
    }
}

TEST_CASE("bad hostname", "[io][http]")
{
    auto request =
        make_get_request(
            "http://f5c12743-1b9a-44ee-91a8-adaed32cc607.bad/status",
            http_header_list());
    try
    {
        perform_simple_request(request);
        FAIL("no exception thrown");
    }
    catch (http_request_failure& e)
    {
        REQUIRE(get_required_error_info<attempted_http_request_info>(e) == request);
        REQUIRE(!get_required_error_info<internal_error_message_info>(e).empty());
    }
}

TEST_CASE("interrupted request", "[io][http]")
{
    struct canceled {};
    struct canceling_check_in : check_in_interface
    {
        void operator()() { throw canceled(); }
    };
    auto request = make_get_request("http://postman-echo.com/delay/10", http_header_list());
    try
    {
        http_connection connection(the_http_request_system);
        canceling_check_in check_in;
        null_progress_reporter reporter;
        connection.perform_request(check_in, reporter, request);
        FAIL("no exception thrown");
    }
    catch (canceled& )
    {
    }
}
