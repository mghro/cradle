#include <cradle/caching/disk_cache.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

#include <cradle/encodings/base64.h>
#include <cradle/fs/file_io.h>
#include <cradle/utilities/testing.h>
#include <cradle/utilities/text.h>

#include <sqlite3.h>

using namespace cradle;

static void
reset_directory(file_path const& dir)
{
    if (exists(dir))
        remove_all(dir);
    create_directory(dir);
}

static void
init_disk_cache(disk_cache& cache, string const& cache_dir = "disk_cache")
{
    reset_directory(cache_dir);

    disk_cache_config config;
    config.directory = some(cache_dir);
    // Given the way that the value strings are generated below, this is
    // enough to hold a little under 20 items (which matters for testing
    // the eviction behavior).
    config.size_limit = 500;

    cache.reset(config);
    REQUIRE(cache.is_initialized());

    auto info = cache.get_summary_info();
    REQUIRE(info.directory == cache_dir);
    REQUIRE(info.entry_count == 0);
    REQUIRE(info.total_size == 0);
}

// Generate some (meaningless) key string for the item with the given ID.
static string
generate_key_string(int item_id)
{
    return "meaningless_key_string_" + lexical_cast<string>(item_id);
}

// Generate some (meaningless) value string for the item with the given ID.
static string
generate_value_string(int item_id)
{
    return "meaningless_value_string_" + lexical_cast<string>(item_id);
}

// Test access to an item. - This simulates access to an item via the disk
// cache. It works whether or not the item is already cached. (It will insert
// it if it's not already there.) It tests various steps along the way,
// including whether or not the cached item was valid.
//
// Since there are two methods of storing data in the cache (inside the
// database or externally in files), this uses the in-database method for
// even-numbered item IDs and the external method for odd-numbered IDs.
//
// The return value indicates whether or not the item was already cached.
//
static bool
test_item_access(disk_cache& cache, int item_id)
{
    auto key = generate_key_string(item_id);
    auto value = generate_value_string(item_id);

    // We're faking the CRC. The cache doesn't care.
    auto computed_crc = uint32_t(item_id) + 1;

    auto entry = cache.find(key);
    if (item_id % 2 == 1)
    {
        // Use external storage.
        if (entry)
        {
            auto cached_contents
                = read_file_contents(cache.get_path_for_id(entry->id));
            REQUIRE(cached_contents == value);
            REQUIRE(entry->crc32 == computed_crc);
            cache.record_usage(entry->id);
            cache.write_usage_records();
            return true;
        }
        else
        {
            auto entry_id = cache.initiate_insert(key);
            dump_string_to_file(cache.get_path_for_id(entry_id), value);
            cache.finish_insert(entry_id, computed_crc);
            return false;
        }
    }
    else
    {
        // Use in-database storage.
        if (entry)
        {
            REQUIRE(entry->value);
            REQUIRE(*entry->value == value);
            cache.record_usage(entry->id);
            cache.write_usage_records();
            return true;
        }
        else
        {
            cache.insert(key, value);
            // Check that it's been added.
            auto new_entry = cache.find(key);
            REQUIRE(new_entry);
            REQUIRE(new_entry->value);
            REQUIRE(*new_entry->value == value);
            // Overwrite it with a dummy value.
            cache.insert(key, "overwritten");
            // Do it all again to test update behavior.
            cache.insert(key, value);
            new_entry = cache.find(key);
            REQUIRE(new_entry);
            REQUIRE(new_entry->value);
            REQUIRE(*new_entry->value == value);
            return false;
        }
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
    REQUIRE(!test_item_access(cache, 1));
    REQUIRE(test_item_access(cache, 1));
}

TEST_CASE("multiple initializations", "[disk_cache]")
{
    disk_cache cache;
    init_disk_cache(cache);
    init_disk_cache(cache, "alt_disk_cache");
    // Test that it can still handle basic operations.
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
    REQUIRE(test_item_access(cache, 1));
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
    // in the cache while other low-numbered entries eventually get evicted.
    for (int i = 2; i != 30; ++i)
    {
        INFO(i)
        REQUIRE(test_item_access(cache, 0));
        REQUIRE(test_item_access(cache, 1));
        REQUIRE(!test_item_access(cache, i));
        // SQLite only maintains millisecond precision on its timestamps, so
        // introduce a delay here to ensure that the timestamps in the cache
        // are unique.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    for (int i = 0; i != 2; ++i)
    {
        // Insert the item and check that it was inserted.
        REQUIRE(!test_item_access(cache, i));
        REQUIRE(test_item_access(cache, i));
        // Remove it.
        {
            auto entry = cache.find(generate_key_string(i));
            if (entry)
                cache.remove_entry(entry->id);
        }
        // Check that it's not there.
        REQUIRE(!test_item_access(cache, i));
    }
}

TEST_CASE("cache summary info", "[disk_cache]")
{
    disk_cache cache;

    int64_t expected_size = 0;
    int64_t expected_count = 0;
    auto check_summary_info = [&]() {
        auto summary = cache.get_summary_info();
        REQUIRE(summary.entry_count == expected_count);
        REQUIRE(summary.total_size == expected_size);
    };

    // Test an empty cache.
    init_disk_cache(cache);
    check_summary_info();

    // Add an entry.
    test_item_access(cache, 0);
    expected_size += generate_value_string(0).length();
    ++expected_count;
    check_summary_info();

    // Add another entry.
    test_item_access(cache, 1);
    expected_size += generate_value_string(1).length();
    ++expected_count;
    check_summary_info();

    // Add another entry.
    test_item_access(cache, 2);
    expected_size += generate_value_string(2).length();
    ++expected_count;
    check_summary_info();

    // Remove an entry.
    {
        auto entry = cache.find(generate_key_string(0));
        if (entry)
            cache.remove_entry(entry->id);
    }
    expected_size -= generate_value_string(0).length();
    --expected_count;
    check_summary_info();
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
        auto entry = cache.find(generate_key_string(0));
        if (entry)
            cache.remove_entry(entry->id);
    }
    // Check the entry list.
    auto entries = cache.get_entry_list();
    // This assumes that the list is in order of last access, which just
    // happens to be the case.
    REQUIRE(entries.size() == 2);
    {
        REQUIRE(entries[0].key == generate_key_string(1));
        REQUIRE(size_t(entries[0].size) == generate_value_string(1).length());
        REQUIRE(!entries[0].in_db);
    }
    {
        REQUIRE(entries[1].key == generate_key_string(2));
        REQUIRE(size_t(entries[1].size) == generate_value_string(2).length());
        REQUIRE(entries[1].in_db);
    }
}

TEST_CASE("corrupt cache", "[disk_cache]")
{
    // Set up an invalid cache directory.
    reset_directory("disk_cache");
    dump_string_to_file("disk_cache/index.db", "invalid database contents");
    file_path extraneous_file("disk_cache/some_other_file");
    dump_string_to_file(extraneous_file, "abc");

    // Check that the cache still initializes and that the extraneous file
    // is removed.
    disk_cache cache;
    init_disk_cache(cache);
    REQUIRE(!exists(extraneous_file));
}

TEST_CASE("incompatible cache", "[disk_cache]")
{
    // Set up a cache directory with an incompatible database version number.
    reset_directory("disk_cache");
    {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open("disk_cache/index.db", &db) == SQLITE_OK);
        REQUIRE(
            sqlite3_exec(db, "pragma user_version = 9600;", 0, 0, 0)
            == SQLITE_OK);
        sqlite3_close(db);
    }
    file_path extraneous_file("disk_cache/some_other_file");
    dump_string_to_file(extraneous_file, "abc");

    // Check that the cache still initializes and that the extraneous file
    // is removed.
    disk_cache cache;
    init_disk_cache(cache);
    REQUIRE(!exists(extraneous_file));
}
