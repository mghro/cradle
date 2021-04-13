#include <cradle/caching/immutable/production.h>

#include <cradle/caching/immutable/internals.h>

namespace cradle {

void
report_immutable_cache_loading_progress(
    immutable_cache& cache_object, id_interface const& key, float progress)
{
    auto& cache = *cache_object.impl;

    std::list<std::weak_ptr<immutable_cache_entry_watcher>> watchers;

    // Update the cache record.
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);

        auto i = cache.records.find(&key);
        if (i == cache.records.end())
            return;

        detail::immutable_cache_record* record = i->second.get();
        record->progress.store(
            encode_progress(progress), std::memory_order_relaxed);
        watchers = record->watchers;
    }

    // Invoke all the watchers outside of the mutex lock.
    for (auto& watcher : watchers)
    {
        if (auto locked = watcher.lock())
            locked->on_progress(progress);
    }
}

void
set_immutable_cache_data(
    immutable_cache& cache_object,
    id_interface const& key,
    untyped_immutable value)
{
    auto& cache = *cache_object.impl;

    std::list<std::weak_ptr<immutable_cache_entry_watcher>> watchers;

    // Update the cache record.
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);

        auto i = cache.records.find(&key);
        if (i == cache.records.end())
            return;

        detail::immutable_cache_record* record = i->second.get();
        record->data = value;
        record->state.store(
            immutable_cache_entry_state::READY, std::memory_order_relaxed);
        record->progress.store(encoded_optional_progress());
        record->job.reset();
        watchers = record->watchers;
    }

    // Invoke all the watchers outside of the mutex lock.
    for (auto& watcher : watchers)
    {
        if (auto locked = watcher.lock())
            locked->on_ready(value);
    }
}

void
report_immutable_cache_loading_failure(
    immutable_cache& cache_object, id_interface const& key)
{
    auto& cache = *cache_object.impl;

    std::list<std::weak_ptr<immutable_cache_entry_watcher>> watchers;

    // Update the cache record.
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);

        auto i = cache.records.find(&key);
        if (i == cache.records.end())
            return;

        detail::immutable_cache_record* record = i->second.get();
        record->state.store(immutable_cache_entry_state::FAILED);
        record->progress.store(encoded_optional_progress());
        record->job.reset();
        watchers = record->watchers;
    }

    // Invoke all the watchers outside of the mutex lock.
    for (auto& watcher : watchers)
    {
        if (auto locked = watcher.lock())
            locked->on_failure();
    }
}

} // namespace cradle
