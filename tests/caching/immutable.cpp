#include <cradle/caching/immutable.h>

#include <cradle/core/immutable.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

// struct set_int_job : composable_background_job
// {
//     set_int_job()
//     {
//     }
//     set_int_job(
//         immutable_cache& cache, alia::id_interface const& id, int value)
//         : cache_(&cache), value_(value)
//     {
//         id_.store(id);
//     }
//     void
//     execute(
//         check_in_interface& check_in, progress_reporter_interface& reporter)
//     {
//         check_in();
//         reporter(0);
//         set_cached_data(*cache_, id_.get(), make_immutable(value_));
//         reporter(1);
//     }
//     memory_cache* cache_;
//     alia::owned_id id_;
//     int value_;
// };

TEST_CASE("basic immutable cache usage", "[immutable_cache]")
{
    immutable_cache cache;

    immutable_cache_ptr<int> p;
    REQUIRE(!p.is_initialized());

    bool p_needed_creation = false;
    p.reset(cache, make_id(0), [&] {
        p_needed_creation = true;
        return background_job_controller();
    });
    REQUIRE(p_needed_creation);
    REQUIRE(p.is_initialized());
    REQUIRE(!p.is_ready());
    REQUIRE(p.is_loading());
    REQUIRE(p.key() == make_id(0));

    bool q_needed_creation = false;
    immutable_cache_ptr<int> q(cache, make_id(1), [&] {
        q_needed_creation = true;
        return background_job_controller();
    });
    REQUIRE(q_needed_creation);
    REQUIRE(q.is_initialized());
    REQUIRE(!q.is_ready());
    REQUIRE(q.is_loading());
    REQUIRE(q.key() == make_id(1));

    bool r_needed_creation = false;
    immutable_cache_ptr<int> r(cache, make_id(0), [&] {
        r_needed_creation = true;
        return background_job_controller();
    });
    REQUIRE(!r_needed_creation);

    p = q;
    REQUIRE(p.is_initialized());
    REQUIRE(!p.is_ready());
    REQUIRE(p.is_loading());
    REQUIRE(p.key() == make_id(1));

    set_immutable_cache_data(cache, make_id(1), make_immutable(12));

    REQUIRE(!p.is_ready());
    REQUIRE(p.is_loading());
    p.update();
    REQUIRE(p.is_ready());
    REQUIRE(!p.is_loading());
    REQUIRE(*p == 12);

    REQUIRE(!q.is_ready());
    REQUIRE(q.is_loading());
    q.update();
    REQUIRE(q.is_ready());
    REQUIRE(!q.is_loading());
    REQUIRE(*q == 12);

    // background_execution_system bg;

    // p.reset(cache, make_id(0));
    // REQUIRE(!p.is_ready());
    // REQUIRE(p.is_nowhere());

    // background_job_interface job;
    // add_background_job(bg, &job, new set_int_job(cache, make_id(0), 4));
    // p.set_job(&job);

    // p.update();
    // while (!p.is_ready())
    // {
    //     REQUIRE(p.state() == cached_data_state::COMPUTING);
    //     p.update();
    // }

    // REQUIRE(!p.is_nowhere());
    // REQUIRE(*p == 4);
}