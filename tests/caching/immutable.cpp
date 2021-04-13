#include <cradle/caching/immutable.h>

#include <sstream>

#include <cradle/core/immutable.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

namespace {

// Sort the entry lists in a cache snapshot so that it's consistent across
// unordered_map implementations.
immutable_cache_snapshot
sort_cache_snapshot(immutable_cache_snapshot snapshot)
{
    auto comparator = [](immutable_cache_entry_snapshot const& a,
                         immutable_cache_entry_snapshot const& b) {
        return a.key < b.key;
    };
    sort(snapshot.in_use.begin(), snapshot.in_use.end(), comparator);
    sort(
        snapshot.pending_eviction.begin(),
        snapshot.pending_eviction.end(),
        comparator);
    return snapshot;
}

} // namespace

TEST_CASE("basic immutable cache usage", "[immutable_cache]")
{
    immutable_cache cache;

    immutable_cache_ptr<int> p;
    {
        INFO("A default-constructed immutable_cache_ptr is uninitialized.");
        REQUIRE(!p.is_initialized());
    }

    {
        INFO(
            "The first time an immutable_cache_ptr is attached to a key, its "
            ":create_job callback is invoked.");
        bool p_needed_creation = false;
        p.reset(cache, make_id(0), [&] {
            p_needed_creation = true;
            return background_job_controller();
        });
        REQUIRE(p_needed_creation);
        // Also check all that all the ptr accessors work.
        REQUIRE(p.is_initialized());
        REQUIRE(!p.is_ready());
        REQUIRE(p.is_loading());
        REQUIRE(p.key() == make_id(0));
    }

    {
        INFO("get_cache_snapshot reflects that entry 0 is loading.");
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0}},
                {}}));
    }

    bool q_needed_creation = false;
    immutable_cache_ptr<int> q(cache, make_id(1), [&] {
        q_needed_creation = true;
        return background_job_controller();
    });
    {
        INFO(
            "The first time an immutable_cache_ptr is attached to a new key, "
            "its :create_job callback is invoked.");
        REQUIRE(q_needed_creation);
    }
    // Also check all that all the ptr accessors work.
    REQUIRE(q.is_initialized());
    REQUIRE(!q.is_ready());
    REQUIRE(q.is_loading());
    REQUIRE(q.progress() == none);
    REQUIRE(q.key() == make_id(1));

    {
        INFO(
            "get_cache_snapshot reflects that there are two entries loading.");
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
                 {"1", immutable_cache_entry_state::LOADING, none, none, 0}},
                {}}));
    }

    bool r_needed_creation = false;
    immutable_cache_ptr<int> r(cache, make_id(0), [&] {
        r_needed_creation = true;
        return background_job_controller();
    });
    {
        INFO(
            "The second time an immutable_cache_ptr is attached to a key, "
            "its :create_job callback is NOT invoked.");
        REQUIRE(!r_needed_creation);
    }

    {
        INFO("get_cache_snapshot shows no change.");
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
                 {"1", immutable_cache_entry_state::LOADING, none, none, 0}},
                {}}));
    }

    {
        INFO("immutable_cache_ptr copying works as expected.");
        p = q;
        REQUIRE(p.is_initialized());
        REQUIRE(!p.is_ready());
        REQUIRE(p.is_loading());
        REQUIRE(p.progress() == none);
        REQUIRE(p.key() == make_id(1));
    }

    {
        INFO(
            "When progress is reported by a producer, both the cache snapshot "
            "and the immutable_cache_ptr reflect the progress once updated.");

        report_immutable_cache_loading_progress(cache, make_id(1), 0.25);

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
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
    }

    {
        INFO(
            "When a cache value is supplied by a producer, both the cache "
            "snapshot and the immutable_cache_ptrs reflect the new value once "
            "updated.");
        set_immutable_cache_data(cache, make_id(1), make_immutable(12));

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
                 {"1",
                  immutable_cache_entry_state::READY,
                  none,
                  some(make_api_type_info_with_integer_type(
                      api_integer_type())),
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

    {
        INFO("immutable_cache_ptr movement works as expected.");

        immutable_cache_ptr<int> inner_p = std::move(p);
        REQUIRE(inner_p.is_initialized());
        REQUIRE(inner_p.is_ready());
        REQUIRE(*inner_p == 12);
        REQUIRE(!p.is_initialized());
    }

    {
        INFO(
            "q wasn't affected by p having its contents moved out, even "
            "though it referenced the same entry.");

        REQUIRE(q.is_ready());
        REQUIRE(*q == 12);
    }

    {
        INFO(
            "The cache snapshot also still reflects that q's value is in "
            "use.");

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0},
                 {"1",
                  immutable_cache_entry_state::READY,
                  none,
                  some(make_api_type_info_with_integer_type(
                      api_integer_type())),
                  sizeof(int)}},
                {}}));
    }

    {
        INFO(
            "Resetting q will change the snapshot to reflect that q's value "
            "is now pending evition.");

        q.reset();

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0}},
                {{"1",
                  immutable_cache_entry_state::READY,
                  none,
                  some(make_api_type_info_with_integer_type(
                      api_integer_type())),
                  sizeof(int)}}}));
    }

    {
        INFO(
            "Clearing unused entries in the cache will clear out q's old "
            "value.");

        clear_unused_entries(cache);

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, none, none, 0}},
                {}}));
    }

    {
        INFO("Immutable cache failure reporting works as expected.");

        report_immutable_cache_loading_failure(cache, make_id(0));

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::FAILED, none, none, 0}},
                {}}));

        r.update();
        REQUIRE(r.is_failed());
    }
}

TEST_CASE("immutable cache entry watching", "[immutable_cache]")
{
    immutable_cache cache;

    struct test_watcher : immutable_cache_entry_watcher
    {
        std::string label;
        std::ostringstream& log;

        test_watcher(std::string label, std::ostringstream& log)
            : label(std::move(label)), log(log)
        {
        }

        void
        on_progress(float progress) override
        {
            log << label << ": on_progress: " << progress << ";";
        }

        void
        on_failure() override
        {
            log << label << ": on_failure;";
        }

        void
        on_ready(untyped_immutable value) override
        {
            log << label << ": on_ready: " << cast_immutable<int>(value)
                << ";";
        }
    };

    std::ostringstream log;
    auto check_log = [&](string const& expected_content) {
        REQUIRE(log.str() == expected_content);
        log.str("");
    };

    immutable_cache_entry_handle p(
        cache,
        make_id(0),
        [&] { return background_job_controller(); },
        std::make_shared<test_watcher>("0", log));

    immutable_cache_entry_handle q(
        cache,
        make_id(1),
        [&] { return background_job_controller(); },
        std::make_shared<test_watcher>("1", log));

    report_immutable_cache_loading_progress(cache, make_id(1), 0.25);
    check_log("1: on_progress: 0.25;");

    // Duplicating the handle also duplicates the watchers.
    auto r = q;
    report_immutable_cache_loading_progress(cache, make_id(1), 0.375);
    check_log("1: on_progress: 0.375;1: on_progress: 0.375;");

    // Resetting one of the handles restores us to one watcher.
    r.reset();
    report_immutable_cache_loading_progress(cache, make_id(1), 0.5);
    check_log("1: on_progress: 0.5;");

    // Moving the handle doesn't change the watcher situation.
    auto s = std::move(r);
    report_immutable_cache_loading_progress(cache, make_id(1), 0.625);
    check_log("1: on_progress: 0.625;");

    set_immutable_cache_data(cache, make_id(1), make_immutable(12));
    check_log("1: on_ready: 12;");

    report_immutable_cache_loading_failure(cache, make_id(0));
    check_log("0: on_failure;");
}
