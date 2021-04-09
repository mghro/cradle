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

immutable_cache_data_status
make_immutable_cache_data_status(immutable_cache_data_state state)
{
    immutable_cache_data_status status;
    status.state = state;
    return status;
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
            immutable_cache_data_state::LOADING, std::memory_order_relaxed);
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

void
update_immutable_cache_data_progress(
    immutable_cache& cache, id_interface const& key, float progress)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);

    auto i = cache.records.find(&key);
    if (i == cache.records.end())
        return;

    immutable_cache_record* record = i->second.get();
    record->progress.store(
        encode_progress(progress), std::memory_order_relaxed);
}

} // namespace

void
update_immutable_cache_data_progress(
    immutable_cache& cache, id_interface const& key, float progress)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);

    auto i = cache.records.find(&key);
    if (i == cache.records.end())
        return;

    immutable_cache_record* record = i->second.get();
    record->progress.store(
        encode_progress(progress), std::memory_order_relaxed);
}

void
untyped_immutable_cache_ptr::reset()
{
    if (r_)
    {
        release_cache_record(r_);
        r_ = 0;
    }
    status_ = make_immutable_cache_data_status(
        immutable_cache_data_state::LOADING);
    key_.clear();
    data_ = untyped_immutable();
}

void
untyped_immutable_cache_ptr::reset(
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
untyped_immutable_cache_ptr::acquire(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<background_job_controller()> const& create_job)
{
    r_ = acquire_cache_record(*cache.impl, key, create_job);
    status_ = make_immutable_cache_data_status(
        immutable_cache_data_state::LOADING);
    update();
    key_.capture(key);
}

void
untyped_immutable_cache_ptr::update()
{
    if (status_.state != immutable_cache_data_state::READY)
    {
        status_.state = r_->state.load(std::memory_order_relaxed);
        status_.progress = r_->progress.load(std::memory_order_relaxed);
        if (status_.state == immutable_cache_data_state::READY)
        {
            std::scoped_lock<std::mutex> lock(r_->owner_cache->mutex);
            data_ = r_->data;
        }
    }
}

void
untyped_immutable_cache_ptr::copy(untyped_immutable_cache_ptr const& other)
{
    r_ = other.r_;
    if (r_)
        acquire_cache_record(r_);
    status_ = other.status_;
    key_ = other.key_;
    data_ = other.data_;
}

void
untyped_immutable_cache_ptr::swap(untyped_immutable_cache_ptr& other)
{
    using std::swap;
    swap(r_, other.r_);
    swap(status_, other.status_);
    swap(data_, other.data_);
    swap(key_, other.key_);
}

} // namespace detail
} // namespace cradle