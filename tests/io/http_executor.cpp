#include <cradle/io/http_executor.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

static http_request_system the_http_request_system;

TEST_CASE("basic http_request_executor usage", "[io][http]")
{
    struct test_job : http_request_job
    {
        http_request request;
        http_response response;

        void
        execute(
            check_in_interface& check_in,
            progress_reporter_interface& reporter)
        {
            this->response = this->connection->perform_request(
                check_in, reporter, this->request);
        }
    };

    {
        detail::background_execution_pool pool;
        detail::initialize_pool<http_request_executor>(pool, 1, [] {
            return http_request_executor(the_http_request_system);
        });
        auto job_ptr = std::make_unique<test_job>();
        auto* job = job_ptr.get();
        job->request = make_get_request(
            "http://postman-echo.com/get?color=navy", http_header_list());
        auto controller = detail::add_background_job(pool, std::move(job_ptr));
        int n = 0;
        while (n < 1000
               && controller.state() != background_job_state::COMPLETED)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ++n;
        }
        REQUIRE(controller.state() == background_job_state::COMPLETED);
        REQUIRE(job->response.status_code == 200);
        auto body = parse_json_response(job->response);
        REQUIRE(
            get_field(cast<dynamic_map>(body), "args")
            == dynamic({{"color", "navy"}}));
        detail::shut_down_pool(pool);
    }
}
