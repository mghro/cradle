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
acquire_cache_record_no_lock(immutable_cache_record* record)
{
    ++record->ref_count;
    auto& evictions = record->owner_cache->eviction_list.records;
    if (record->eviction_list_iterator != evictions.end())
    {
        assert(record->ref_count == 1);
        remove_from_eviction_list(*record->owner_cache, record);
    }
}

immutable_cache_record*
acquire_cache_record(
    immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job)
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
    acquire_cache_record_no_lock(record);
    return record;
}

void
acquire_cache_record(immutable_cache_record* record)
{
    std::scoped_lock<std::mutex> lock(record->owner_cache->mutex);
    acquire_cache_record_no_lock(record);
}

void
add_to_eviction_list(immutable_cache& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator == list.records.end());
    record->eviction_list_iterator
        = list.records.insert(list.records.end(), record);
    if (record->data.ptr)
        list.total_size += record->data.ptr->deep_size();
}

void
release_cache_record(immutable_cache_record* record)
{
    auto& cache = *record->owner_cache;
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);
        --record->ref_count;
        if (record->ref_count == 0)
            add_to_eviction_list(cache, record);
    }
}

} // namespace

// IMMUTABLE_CACHE_ENTRY_HANDLE

void
immutable_cache_entry_handle::reset()
{
    if (record_)
    {
        release_cache_record(record_);
        record_ = nullptr;
    }
    key_.clear();
}

void
immutable_cache_entry_handle::reset(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job)
{
    if (!key_.matches(key))
    {
        this->reset();
        this->acquire(cache, key, create_job);
    }
}

void
immutable_cache_entry_handle::acquire(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job)
{
    record_ = acquire_cache_record(*cache.impl, key, create_job);
    key_.capture(key);
}

void
immutable_cache_entry_handle::copy(immutable_cache_entry_handle const& other)
{
    record_ = other.record_;
    if (record_)
        acquire_cache_record(record_);
    key_ = other.key_;
}

void
immutable_cache_entry_handle::move_in(immutable_cache_entry_handle&& other)
{
    record_ = other.record_;
    other.record_ = nullptr;
    key_ = std::move(other.key_);
}

void
immutable_cache_entry_handle::swap(immutable_cache_entry_handle& other)
{
    using std::swap;
    swap(record_, other.record_);
    swap(key_, other.key_);
}

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
