#include <cradle/caching/immutable/consumption.h>

#include <cradle/caching/immutable/internals.h>

namespace cradle {
namespace detail {

namespace {

void
remove_from_eviction_list(
    immutable_cache& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator != list.records.end());
    list.records.erase(record->eviction_list_iterator);
    record->eviction_list_iterator = list.records.end();
    if (record->data.ptr)
        list.total_size -= record->data.ptr->deep_size();
}

void
acquire_cache_record_no_lock(
    immutable_cache_record* record,
    std::shared_ptr<immutable_cache_entry_watcher> const& watcher)
{
    ++record->ref_count;
    auto& evictions = record->owner_cache->eviction_list.records;
    if (record->eviction_list_iterator != evictions.end())
    {
        assert(record->ref_count == 1);
        remove_from_eviction_list(*record->owner_cache, record);
    }
    if (watcher)
        record->watchers.push_back(watcher);
}

immutable_cache_record*
acquire_cache_record(
    immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job,
    std::shared_ptr<immutable_cache_entry_watcher> const& watcher)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    cache_record_map::iterator i = cache.records.find(&key);
    if (i == cache.records.end())
    {
        auto record = std::make_unique<immutable_cache_record>();
        record->owner_cache = &cache;
        record->eviction_list_iterator = cache.eviction_list.records.end();
        record->key.capture(key);
        record->state.store(
            immutable_cache_entry_state::LOADING, std::memory_order_relaxed);
        record->progress.store(
            encoded_optional_progress(), std::memory_order_relaxed);
        record->ref_count = 0;
        record->job = create_job();
        i = cache.records.emplace(&*record->key, std::move(record)).first;
    }
    immutable_cache_record* record = i->second.get();
    acquire_cache_record_no_lock(record, watcher);
    return record;
}

void
acquire_cache_record(
    immutable_cache_record* record,
    std::shared_ptr<immutable_cache_entry_watcher> const& watcher)
{
    std::scoped_lock<std::mutex> lock(record->owner_cache->mutex);
    acquire_cache_record_no_lock(record, watcher);
}

void
add_to_eviction_list(immutable_cache& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator == list.records.end());
    record->eviction_list_iterator
        = list.records.insert(list.records.end(), record);
    if (record->data.ptr)
    {
        list.total_size += record->data.ptr->deep_size();
    }
}

bool
same_owner(
    std::shared_ptr<immutable_cache_entry_watcher> const& shared,
    std::weak_ptr<immutable_cache_entry_watcher> const& weak)
{
    return !shared.owner_before(weak) && !weak.owner_before(shared);
}

void
release_cache_record(
    immutable_cache_record* record,
    std::shared_ptr<immutable_cache_entry_watcher> const& watcher)
{
    auto& cache = *record->owner_cache;
    bool do_lru_eviction = false;
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);
        --record->ref_count;
        if (watcher)
        {
            record->watchers.erase(find_if(
                record->watchers.begin(),
                record->watchers.end(),
                [&](auto const& ptr) { return same_owner(watcher, ptr); }));
        }
        if (record->ref_count == 0)
        {
            add_to_eviction_list(cache, record);
            do_lru_eviction = true;
        }
    }
    if (do_lru_eviction)
        reduce_memory_cache_size(cache, cache.config.unused_size_limit);
}

} // namespace
} // namespace detail

// IMMUTABLE_CACHE_ENTRY_HANDLE

void
immutable_cache_entry_handle::reset()
{
    if (record_)
    {
        detail::release_cache_record(record_, watcher_);
        record_ = nullptr;
    }
    key_.clear();
    watcher_.reset();
}

void
immutable_cache_entry_handle::reset(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job,
    std::shared_ptr<immutable_cache_entry_watcher> watcher)
{
    if (!key_.matches(key))
    {
        this->reset();
        this->acquire(cache, key, create_job, std::move(watcher));
    }
}

void
immutable_cache_entry_handle::acquire(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job,
    std::shared_ptr<immutable_cache_entry_watcher> watcher)
{
    record_
        = detail::acquire_cache_record(*cache.impl, key, create_job, watcher);
    key_.capture(key);
    watcher_ = std::move(watcher);
}

void
immutable_cache_entry_handle::copy(immutable_cache_entry_handle const& other)
{
    record_ = other.record_;
    watcher_ = other.watcher_;
    if (record_)
        detail::acquire_cache_record(record_, watcher_);
    key_ = other.key_;
}

void
immutable_cache_entry_handle::move_in(immutable_cache_entry_handle&& other)
{
    record_ = other.record_;
    other.record_ = nullptr;
    key_ = std::move(other.key_);
    watcher_ = std::move(other.watcher_);
}

void
immutable_cache_entry_handle::swap(immutable_cache_entry_handle& other)
{
    using std::swap;
    swap(record_, other.record_);
    swap(key_, other.key_);
    swap(watcher_, other.watcher_);
}

namespace detail {

// UNTYPED_IMMUTABLE_CACHE_PTR

void
untyped_immutable_cache_ptr::reset()
{
    handle_.reset();
    state_ = immutable_cache_entry_state::LOADING;
    progress_ = encoded_optional_progress();
    data_ = untyped_immutable();
}

void
untyped_immutable_cache_ptr::reset(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job)
{
    handle_.reset(cache, key, create_job);
    state_ = immutable_cache_entry_state::LOADING;
    update();
}

void
untyped_immutable_cache_ptr::update()
{
    if (state_ != immutable_cache_entry_state::READY)
    {
        auto* record = handle_.record();
        state_ = record->state.load(std::memory_order_relaxed);
        progress_ = record->progress.load(std::memory_order_relaxed);
        if (state_ == immutable_cache_entry_state::READY)
        {
            std::scoped_lock<std::mutex> lock(record->owner_cache->mutex);
            data_ = record->data;
        }
    }
}

} // namespace detail
} // namespace cradle
