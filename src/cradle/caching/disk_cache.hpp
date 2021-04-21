#ifndef CRADLE_CACHING_DISK_CACHE_HPP
#define CRADLE_CACHING_DISK_CACHE_HPP

#include <cradle/core.h>
#include <cradle/fs/types.hpp>

#include <memory>
#include <vector>

namespace cradle {

// A disk cache is used for caching immutable data on the local hard drive to
// avoid redownloading it or recomputing it.

// The cache is implemented as a directory of files with an SQLite index
// database file that aids in tracking usage information.

// Note that a disk cache will generate exceptions any time an operation fails.
// Of course, since caching is by definition not essential to the correct
// operation of a program, there should always be a way to recover from these
// exceptions.

// A cache is internally protected by a mutex, so it can be used concurrently
// from multiple threads.

api(struct)
struct disk_cache_config
{
    optional<std::string> directory;
    integer size_limit;
};

api(struct)
struct disk_cache_info
{
    // the directory where the cache is stored
    std::string directory;

    // the number of entries currently stored in the cache
    integer entry_count;

    // the total size (in bytes)
    integer total_size;
};

api(struct)
struct disk_cache_entry
{
    // the key for the entry
    std::string key;

    // the internal numeric ID of the entry within the cache
    integer id;

    // true iff the entry is stored directly in the database
    bool in_db;

    // the value associated with the entry - This may be omitted, depending
    // on how the entry is stored in the cache and how this info was
    // queried.
    omissible<std::string> value;

    // the size of the entry (in bytes)
    integer size;

    // a 32-bit CRC of the contents of the entry
    uint32_t crc32;
};

// This exception indicates a failure in the operation of the disk cache.
CRADLE_DEFINE_EXCEPTION(disk_cache_failure)
// This provides the path to the disk cache directory.
CRADLE_DEFINE_ERROR_INFO(file_path, disk_cache_path)
// This exception also provides internal_error_message_info.

struct disk_cache_impl;

struct disk_cache
{
    // The default constructor creates an invalid disk cache that must be
    // initialized via reset().
    disk_cache();

    // Create a disk cache that's initialized with the given config.
    disk_cache(disk_cache_config const& config);

    ~disk_cache();

    // Reset the cache with a new config.
    // After a successful call to this, the cache is considered initialized.
    void
    reset(disk_cache_config const& config);

    // Reset the cache to an uninitialized state.
    void
    reset();

    // Is the cache initialized?
    bool
    is_initialized()
    {
        return impl_ ? true : false;
    }

    // The rest of this interface should only be used if is_initialized()
    // returns true.

    // Get summary information about the cache.
    disk_cache_info
    get_summary_info();

    // Get a list of all entries in the cache.
    // Note that none of the returned entries will include values.
    std::vector<disk_cache_entry>
    get_entry_list();

    // Remove an individual entry from the cache.
    void
    remove_entry(int64_t id);

    // Clear the cache of all data.
    void
    clear();

    // Look up a key in the cache.
    //
    // The returned entry is valid iff there's a valid entry associated with
    // :key.
    //
    // Note that for entries stored directly in the database, this also
    // retrieves the value associated with the entry.
    //
    optional<disk_cache_entry>
    find(string const& key);

    // Add a small entry to the cache.
    // This should only be used on entries that are known to be smaller than
    // a few kB. Below this level, it is more efficient (both in time and
    // storage) to store data directly in the SQLite database.
    void
    insert(string const& key, string const& value);

    // Add an arbitrarily large entry to the cache.
    //
    // This is a two-part process.
    // First, you initiate the insert to get the ID for the entry.
    // Then, once the entry is written to disk, you finish the insert.
    // (If an error occurs in between, it's OK to simply abandon the entry,
    // as it will be marked as invalid initially.)
    //
    int64_t
    initiate_insert(string const& key);
    void
    finish_insert(int64_t id, uint32_t crc32);

    // Given an ID within the cache, this computes the path of the file that
    // would store the data associated with that ID (assuming that entry were
    // actually stored in a file rather than in the database).
    file_path
    get_path_for_id(int64_t id);

    // Record that an ID within the cache was just used.
    // When a lot of small objects are being read from the cache, the calls to
    // record_usage() can slow down the loading process.
    // To address this, calls are buffered and sent all at once when the cache
    // is idle.
    void
    record_usage(int64_t id);

    // If you know that the cache is idle, you can call this to force the cache
    // to write out its buffered usage records. (This is automatically called
    // when the cache is destructed.)
    void
    write_usage_records();

    // Another approach is to call this function periodically.
    // It checks to see how long it's been since the cache was last used, and
    // if the cache appears idle, it automatically writes the usage records.
    void
    do_idle_processing();

 private:
    std::unique_ptr<disk_cache_impl> impl_;
};

} // namespace cradle

#endif
