#include <cradle/caching/immutable/internals.h>

#include <mutex>

#include <cradle/utilities/text.h>

namespace cradle {

namespace detail {

void
reduce_memory_cache_size(immutable_cache& cache, size_t desired_size)
{
    // We need to keep the jobs around until after the mutex is released
    // because they may recursively release other records.
    std::list<background_job_controller> evicted_jobs;
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);
        while (!cache.eviction_list.records.empty()
               && cache.eviction_list.total_size > desired_size)
        {
            auto const& i = cache.eviction_list.records.front();
            auto data_size = i->data.ptr ? i->data.ptr->deep_size() : 0;
            evicted_jobs.push_back(std::move(i->job));
            cache.records.erase(&*i->key);
            cache.eviction_list.records.pop_front();
            cache.eviction_list.total_size -= data_size;
        }
    }
}

} // namespace detail
} // namespace cradle
