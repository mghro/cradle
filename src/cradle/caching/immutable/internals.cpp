#include <cradle/caching/immutable/internals.h>

#include <mutex>

namespace cradle {

namespace detail {

void
reduce_memory_cache_size(immutable_cache& cache, int desired_size)
{
    // We need to keep the jobs around until after the mutex is released
    // because they may recursively release other records.
    std::list<std::unique_ptr<immutable_cache_job_interface>> evicted_jobs;
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);
        while (!cache.eviction_list.records.empty()
               && cache.eviction_list.total_size
                      > size_t(desired_size) * 0x100000)
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

immutable_cache_job_interface*
get_job_interface(immutable_cache_record* record)
{
    std::scoped_lock<std::mutex> lock(record->owner_cache->mutex);
    return record->job.get();
}

void
set_cached_data(
    immutable_cache& cache,
    id_interface const& key,
    untyped_immutable const& data)
{
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);

        auto i = cache.records.find(&key);
        if (i == cache.records.end())
            return;

        immutable_cache_record* record = i->second.get();
        record->data = data;
        record->state.store(
            immutable_cache_data_state::READY, std::memory_order_relaxed);
        // Ideally, the job controller should be reset here, since we don't
        // really need it anymore, but this causes some tricky synchronization
        // issues with the UI code that's observing it.
        // record->job->reset();
    }

    // TODO!!
    // Setting this data could've made it possible for any of the waiting
    // calculation jobs to run.
    // wake_up_waiting_jobs(
    //     *system.impl_->pools[int(background_job_queue_type::CALCULATION)]
    //          .queue);
}

void
reset_cached_data(immutable_cache& cache, id_interface const& key)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);

    auto i = cache.records.find(&key);
    if (i == cache.records.end())
        return;

    immutable_cache_record* record = i->second.get();
    record->state.store(
        immutable_cache_data_state::LOADING, std::memory_order_relaxed);
}

string
get_key_string(immutable_cache_record* record)
{
    return boost::lexical_cast<std::string>(*record->key);
}

} // namespace detail
} // namespace cradle
