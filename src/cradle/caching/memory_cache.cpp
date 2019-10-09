#include <cradle/caching/memory_cache.hpp>

#include <cassert>
#include <list>
#include <unordered_map>

#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>

#include <cradle/core/hashing.hpp>

namespace cradle {

struct cache_item
{
    std::string key_hash;
    memory_cache_ptr value;
    size_t size;
};

struct memory_cache_impl
{
    // the config for this cache
    memory_cache_config config;
    // list for storing cached items and tracking LRU order
    std::list<cache_item> item_list;
    // map for associating keys with values
    std::unordered_map<std::string, decltype(item_list.begin())> item_map;
    // the current total size of items in the cache
    size_t total_size = 0;
    // protects all access to the cache
    boost::mutex mutex;
};

namespace impl {

static void
clean(memory_cache_impl& cache)
{
    while (cache.total_size > size_t(cache.config.size_limit))
    {
        auto const& lru_item = cache.item_list.back();
        cache.item_map.erase(lru_item.key_hash);
        cache.total_size -= lru_item.size;
        cache.item_list.pop_back();
    }
}

static void
remove(memory_cache_impl& cache, std::string const& key_hash)
{
    auto i = cache.item_map.find(key_hash);
    if (i != cache.item_map.end())
    {
        cache.total_size -= i->second->size;
        cache.item_list.erase(i->second);
        cache.item_map.erase(i);
    }
}

void
put(memory_cache_impl& cache, dynamic const& key, dynamic&& value)
{
    auto key_hash = sha256_hash(key);

    // It should be rare that a duplicate item is added to the cache, but it
    // could happen in race conditions, so check for and handle it.
    remove(cache, key_hash);

    size_t item_size
        = sizeof(cache_item) + 64 /* for key hash */ + deep_sizeof(value);

    cache.item_list.push_front(
        cache_item{key_hash,
                   std::make_shared<dynamic const>(std::move(value)),
                   item_size});
    cache.item_map.insert(std::make_pair(key_hash, cache.item_list.begin()));
    cache.total_size += item_size;

    clean(cache);
};

memory_cache_ptr
get(memory_cache_impl& cache, dynamic const& key)
{
    auto key_hash = sha256_hash(key);
    auto i = cache.item_map.find(key_hash);
    if (i == cache.item_map.end())
        return memory_cache_ptr();
    cache.item_list.splice(cache.item_list.begin(), cache.item_list, i->second);
    return i->second->value;
}

} // namespace impl

memory_cache::memory_cache(memory_cache_config const& config)
{
    impl_ = new memory_cache_impl();
    impl_->config = config;
}

memory_cache::~memory_cache()
{
    delete this->impl_;
}

void
memory_cache::put(dynamic const& key, dynamic&& value)
{
    boost::lock_guard<boost::mutex> lock(impl_->mutex);
    impl::put(*impl_, key, std::move(value));
}

memory_cache_ptr
memory_cache::get(dynamic const& key)
{
    boost::lock_guard<boost::mutex> lock(impl_->mutex);
    return impl::get(*impl_, key);
}

} // namespace cradle
