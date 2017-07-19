#include <cradle/disk_cache.hpp>

#include <boost/filesystem/operations.hpp>

#include <cradle/core/testing.hpp>
#include <cradle/io/base64.hpp>

using namespace cradle;

TEST_CASE("uninitialized disk cache", "[disk_cache]")
{
    disk_cache cache;
    REQUIRE(!cache.is_initialized());
    try
    {
        cache.clear();
        FAIL("no exception thrown");
    }
    catch (disk_cache_uninitialized&)
    {
    }
}

void static
init_disk_cache(disk_cache& cache, string const& cache_dir = "disk_cache")
{
    if (boost::filesystem::exists(cache_dir))
        boost::filesystem::remove_all(cache_dir);

    disk_cache_config config;
    config.directory = some(cache_dir);
    config.size_limit = 600;

    cache.reset(config);
    REQUIRE(cache.is_initialized());

    auto info = cache.get_summary_info();
    REQUIRE(info.directory == cache_dir);
    REQUIRE(info.entry_count == 0);
    REQUIRE(info.total_size == 0);
}

// Generate some (meaningless) key string for the item with the given ID.
string static
generate_key_string(int item_id)
{
    return "meaningless_key_string_" + lexical_cast<string>(item_id);
}

// Generate some (meaningless) value string for the item with the given ID.
// The string length is chosen so that the cache fits a reasonable number (tens) of items.
string static
generate_value_string(int item_id)
{
    return "meaningless_value_string_" + lexical_cast<string>(item_id);
}

// Test access to an item. - This simulates access to an item via the disk
// cache. It works whether or not the item is already cached. (It will insert
// it if it's not already there.) It tests various steps along the way,
// including whether or not the cached item was valid.
// The return value indicates whether or not the item was already cached.
bool static
test_item_access(disk_cache& cache, int item_id)
{
    auto key = generate_key_string(item_id);
    auto value = generate_value_string(item_id);

    // We're faking the CRC. The cache doesn't care.
    auto computed_crc = uint32_t(item_id) + 1;

    int64_t entry_id;
    uint32_t stored_crc;
    if (cache.entry_exists(key, &entry_id, &stored_crc))
    {
        auto cached_contents = get_file_contents(cache.get_path_for_id(entry_id));
        REQUIRE(stored_crc == computed_crc);
        cache.record_usage(entry_id);
        cache.write_usage_records();
        return true;
    }
    else
    {
        entry_id = cache.initiate_insert(key);
        {
            auto entry_path = cache.get_path_for_id(entry_id);
            std::ofstream output;
            open(output, entry_path, std::ios::out | std::ios::trunc | std::ios::binary);
            output << value;
        }
        cache.finish_insert(entry_id, computed_crc);
        return false;
    }
}

TEST_CASE("resetting", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    cache.reset();
    REQUIRE(!cache.is_initialized());
}

TEST_CASE("simple item access", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    // The first time, it shouldn't be in the cache, but the second time, it
    // should be.
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 0));
}

TEST_CASE("multiple initializations", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    init_disk_cache(cache, "alt_disk_cache");
    // Test that it can still handle basic operations.
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 0));
}

TEST_CASE("clearing", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 1));
    cache.clear();
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
}

TEST_CASE("LRU removal", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    test_item_access(cache, 0);
    test_item_access(cache, 1);
    // This pattern of access should ensure that entries 0 and 1 always remain
    // in the cache while lower numbered entries eventually get evicted.
    for (int i = 2; i != 40; ++i)
    {
        REQUIRE(test_item_access(cache, 0));
        REQUIRE(test_item_access(cache, 1));
        REQUIRE(!test_item_access(cache, i));
    }
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 1));
    for (int i = 2; i != 10; ++i)
    {
        REQUIRE(!test_item_access(cache, i));
    }
}

TEST_CASE("manual entry removal", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    // Insert the item and check that it was inserted.
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 0));
    // Remove it.
    {
        int64_t entry_id;
        uint32_t stored_crc;
        if (cache.entry_exists(generate_key_string(0), &entry_id, &stored_crc))
        {
            cache.remove_entry(entry_id);
        }
    }
    // Check that it's not there.
    REQUIRE(!test_item_access(cache, 0));
}

TEST_CASE("cache entry list", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    test_item_access(cache, 0);
    test_item_access(cache, 1);
    test_item_access(cache, 2);
    // Remove an entry.
    {
        int64_t entry_id;
        uint32_t stored_crc;
        if (cache.entry_exists(generate_key_string(0), &entry_id, &stored_crc))
        {
            cache.remove_entry(entry_id);
        }
    }
    // Check the entry list.
    auto entries = cache.get_entry_list();
    // This assumes that the list is in order of last access, which just
    // happens to be the case.
    REQUIRE(entries.size() == 2);
    {
        REQUIRE(entries[0].key == generate_key_string(1));
        REQUIRE(size_t(entries[0].size) == generate_value_string(1).length());
    }
    {
        REQUIRE(entries[1].key == generate_key_string(2));
        REQUIRE(size_t(entries[1].size) == generate_value_string(2).length());
    }
}