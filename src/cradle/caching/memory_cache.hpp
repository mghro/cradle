#ifndef CRADLE_CACHING_MEMORY_CACHE_HPP
#define CRADLE_CACHING_MEMORY_CACHE_HPP

#include <cradle/caching/types.hpp>
#include <cradle/core/dynamic.hpp>

namespace cradle {

// A memory_cache is used for caching (possibly large) immutable data in memory.
//
// The cache follows an LRU policy.
//
// The cache is internally protected by a mutex, so it can be used concurrently
// from multiple threads.

typedef std::shared_ptr<dynamic const> memory_cache_ptr;

struct memory_cache_impl;

struct memory_cache : boost::noncopyable
{
    memory_cache(memory_cache_config const& config);

    ~memory_cache();

    void
    put(dynamic const& key, dynamic&& value);

    memory_cache_ptr
    get(dynamic const& key);

 private:
    memory_cache_impl* impl_;
};

} // namespace cradle

#endif
