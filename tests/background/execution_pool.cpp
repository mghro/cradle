#include <cradle/background/execution_pool.h>

#include <sstream>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("basic execution pool", "[background]")
{
    struct test_job : background_job_interface
    {
        bool executed = false;

        void
        execute(
            check_in_interface& check_in,
            progress_reporter_interface& reporter) override
        {
            this->executed = true;
        }
    };

    {
        detail::background_execution_pool pool;
        detail::initialize_pool(pool, detail::basic_executor(), 1);
        auto job_ptr = std::make_unique<test_job>();
        auto* job = job_ptr.get();
        detail::add_background_job(pool, std::move(job_ptr));
        int n = 0;
        while (n < 100 && !job->executed)
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        REQUIRE(job->executed);
        detail::shut_down_pool(pool);
    }
}
