#include <cradle/caching/immutable.h>

#include <cradle/core/immutable.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

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

    REQUIRE(
        get_cache_snapshot(cache)
        == (immutable_cache_snapshot{
            {{"0", immutable_cache_entry_state::LOADING, none, none, 0}},
            {}}));

    bool q_needed_creation = false;
    immutable_cache_ptr<int> q(cache, make_id(1), [&] {
        q_needed_creation = true;
        return background_job_controller();
    });
    REQUIRE(q_needed_creation);
    REQUIRE(q.is_initialized());
    REQUIRE(!q.is_ready());
    REQUIRE(q.is_loading());
    REQUIRE(q.progress() == none);
    REQUIRE(q.key() == make_id(1));

    REQUIRE(
        get_cache_snapshot(cache)
        == (immutable_cache_snapshot{
            {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
             {"1", immutable_cache_entry_state::LOADING, none, none, 0}},
            {}}));

    bool r_needed_creation = false;
    immutable_cache_ptr<int> r(cache, make_id(0), [&] {
        r_needed_creation = true;
        return background_job_controller();
    });
    REQUIRE(!r_needed_creation);

    REQUIRE(
        get_cache_snapshot(cache)
        == (immutable_cache_snapshot{
            {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
             {"1", immutable_cache_entry_state::LOADING, none, none, 0}},
            {}}));

    p = q;
    REQUIRE(p.is_initialized());
    REQUIRE(!p.is_ready());
    REQUIRE(p.is_loading());
    REQUIRE(p.progress() == none);
    REQUIRE(p.key() == make_id(1));

    report_immutable_cache_loading_progress(cache, make_id(1), 0.25);

    REQUIRE(
        get_cache_snapshot(cache)
        == (immutable_cache_snapshot{
            {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
             {"1",
              immutable_cache_entry_state::LOADING,
              some(0.25f),
              none,
              0}},
            {}}));

    p.update();
    REQUIRE(p.progress() == some(0.25));

    set_immutable_cache_data(cache, make_id(1), make_immutable(12));

    REQUIRE(
        get_cache_snapshot(cache)
        == (immutable_cache_snapshot{
            {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
             {"1",
              immutable_cache_entry_state::READY,
              none,
              some(make_api_type_info_with_integer_type(api_integer_type())),
              sizeof(int)}},
            {}}));

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
}
