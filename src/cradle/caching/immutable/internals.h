#ifndef CRADLE_CACHING_IMMUTABLE_INTERNALS_H
#define CRADLE_CACHING_IMMUTABLE_INTERNALS_H

#include <atomic>
#include <list>
#include <mutex>
#include <unordered_map>

#include <cradle/background/job.h>
#include <cradle/caching/immutable/cache.hpp>
#include <cradle/caching/immutable/consumption.h>

namespace cradle {

struct immutable_cache_entry_watcher;

namespace detail {

struct immutable_cache;

struct immutable_cache_record
{
    // These remain constant for the life of the record.
    immutable_cache* owner_cache;
    captured_id key;

    // All of the following fields are protected by the cache mutex. The only
    // exception is that the :state and :progress fields can be polled for
    // informational purposes. However, before accessing any other fields based
    // on the value of :state, you should acquire the mutex and recheck state.

    std::atomic<immutable_cache_entry_state> state;

    std::atomic<encoded_optional_progress> progress;

    // This is a count of how many active pointers reference this data.
    // If this is 0, the data is just hanging around because it was recently
    // used, in which case :eviction_list_iterator points to this record's
    // entry in the eviction list.
    unsigned ref_count;

    // (See :ref_count comment.)
    std::list<immutable_cache_record*>::iterator eviction_list_iterator;

    // a list of watchers
    std::list<std::weak_ptr<immutable_cache_entry_watcher>> watchers;

    // If state is LOADING, this is the associated job.
    background_job_controller job;

    // If state is READY, this is the associated data.
    untyped_immutable data;
};

typedef std::unordered_map<
    id_interface const*,
    // Atomics unfortunately aren't movable, so for now we just store cache
    // records by pointer.
    std::unique_ptr<immutable_cache_record>,
    id_interface_pointer_hash,
    id_interface_pointer_equality_test>
    cache_record_map;

struct cache_record_eviction_list
{
    std::list<immutable_cache_record*> records;
    size_t total_size;
    cache_record_eviction_list() : total_size(0)
    {
    }
};

struct immutable_cache : noncopyable
{
    immutable_cache_config config;
    cache_record_map records;
    cache_record_eviction_list eviction_list;
    std::mutex mutex;
};

// Evict unused entries (in LRU order) until the total size of unused entries
// in the cache is at most :desired_size (in bytes).
void
reduce_memory_cache_size(immutable_cache& cache, size_t desired_size);

} // namespace detail
} // namespace cradle

#endif
