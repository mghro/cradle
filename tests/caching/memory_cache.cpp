#include <cradle/caching/memory_cache.hpp>

#include <cradle/core/testing.hpp>

using namespace cradle;

TEST_CASE("simple caching", "[caching][memory_cache]")
{
    memory_cache cache(memory_cache_config{4096});
    cache.put(dynamic("foo"), dynamic(integer(17)));
    auto cached = cache.get(dynamic("foo"));
    REQUIRE(cached);
    REQUIRE(*cached == dynamic(integer(17)));
}

TEST_CASE("LRU eviction", "[caching][memory_cache]")
{
    // This cache should fit two test items (but not three).
    memory_cache cache(memory_cache_config{320});

    cache.put(dynamic("foo"), dynamic(integer(17)));
    {
        auto foo = cache.get(dynamic("foo"));
        REQUIRE(foo);
        REQUIRE(*foo == dynamic(integer(17)));
    }

    cache.put(dynamic("bar"), dynamic(integer(5)));
    {
        auto bar = cache.get(dynamic("bar"));
        REQUIRE(bar);
        REQUIRE(*bar == dynamic(integer(5)));
        auto foo = cache.get(dynamic("foo"));
        REQUIRE(foo);
        REQUIRE(*foo == dynamic(integer(17)));
    }

    cache.put(dynamic("baz"), dynamic(integer(1)));
    {
        auto baz = cache.get(dynamic("baz"));
        REQUIRE(baz);
        REQUIRE(*baz == dynamic(integer(1)));
        auto foo = cache.get(dynamic("foo"));
        REQUIRE(foo);
        REQUIRE(*foo == dynamic(integer(17)));
        // bar was LRU, so it should've been evicted.
        auto bar = cache.get(dynamic("bar"));
        REQUIRE(!bar);
    }
}
