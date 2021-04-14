#ifndef CRADLE_CACHING_IMMUTABLE_CACHE_HPP
#define CRADLE_CACHING_IMMUTABLE_CACHE_HPP

#include <memory>

#include <cradle/core.h>

namespace cradle {

namespace detail {

struct immutable_cache;

} // namespace detail

api(struct)
struct immutable_cache_config
{
    // The maximum amount of memory to use for caching results that are no
    // longer in use, in bytes.
    integer unused_size_limit;
};

struct immutable_cache
{
    // The default constructor creates an invalid cache that must be
    // initialized via reset().
    immutable_cache();

    // Create a cache that's initialized with the given config.
    immutable_cache(immutable_cache_config config);

    ~immutable_cache();

    // Reset the cache with a new config.
    // After a successful call to this, the cache is considered initialized.
    void
    reset(immutable_cache_config config);

    // Reset the cache to an uninitialized state.
    void
    reset();

    // Is the cache initialized?
    bool
    is_initialized()
    {
        return impl ? true : false;
    }

    std::unique_ptr<detail::immutable_cache> impl;
};

api(enum)
enum class immutable_cache_entry_state
{
    // The data isn't available yet, but it's somewhere in the process of being
    // loaded/retrieved/computed. The caller should expect that the data will
    // transition to READY without any further intervention.
    LOADING,

    // The data is available.
    READY,

    // The data failed to compute, but it could potentially be retried through
    // some external means.
    FAILED
};

api(struct)
struct immutable_cache_entry_status
{
    immutable_cache_entry_state state;

    // :progress is only valid if state is LOADING, but still optional even
    // then.
    optional<float> progress;
};

api(struct)
struct immutable_cache_entry_snapshot
{
    // the key associated with this entry
    string key;

    // the state of this entry
    immutable_cache_entry_state state;

    // progress in loading the data - possibly valid iff state is LOADING
    optional<float> progress;

    // type info for the cached data - valid iff state is READY
    optional<api_type_info> type_info;

    // size of the cached data - valid iff state is READY, 0 otherwise
    size_t size;
};

api(struct)
struct immutable_cache_snapshot
{
    // cache entries that are currently in use
    std::vector<immutable_cache_entry_snapshot> in_use;

    // cache entries that are no longer in use and will be evicted when
    // necessary
    std::vector<immutable_cache_entry_snapshot> pending_eviction;
};

// Get a snapshot of the contents of an immutable memory cache.
immutable_cache_snapshot
get_cache_snapshot(immutable_cache& cache);

// Clear unused entries from the cache.
void
clear_unused_entries(immutable_cache& cache);

} // namespace cradle

#endif
