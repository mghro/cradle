#include <cradle/caching/immutable_cache.h>

#include <atomic>
#include <list>
#include <mutex>
#include <unordered_map>

namespace cradle {

struct immutable_cache;

struct immutable_cache_record
{
    // These remain constant for the life of the record.
    immutable_cache* owner_cache;
    captured_id key;

    // All of the following fields are protected by the cache mutex. The only
    // exception is that the state and progress fields can be polled for
    // informational purposes. However, before accessing any other fields based
    // on the value of state, you should acquire the mutex and recheck state.

    std::atomic<immutable_cache_data_state> state;

    std::atomic<encoded_optional_progress> progress;

    // This is a count of how many active pointers reference this data.
    // If this is 0, the data is just hanging around because it was recently
    // used, in which case eviction_list_iterator points to this record's
    // entry in the eviction list.
    unsigned ref_count;

    std::list<immutable_cache_record*>::iterator eviction_list_iterator;

    // If state is LOADING, this is the associated job.
    std::unique_ptr<immutable_cache_job_interface> job;

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

struct immutable_cache : boost::noncopyable
{
    cache_record_map records;
    cache_record_eviction_list eviction_list;
    std::mutex mutex;
};

static void
remove_from_eviction_list(
    immutable_cache& cache, immutable_cache_record* record);

static void
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

static immutable_cache_data_status
make_immutable_cache_data_status(immutable_cache_data_state state)
{
    immutable_cache_data_status status;
    status.state = state;
    return status;
}

immutable_cache_record*
acquire_cache_record(immutable_cache& cache, id_interface const& key)
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
        // TODO
        // record.job.reset(new background_job_controller);
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

static void
add_to_eviction_list(immutable_cache& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator == list.records.end());
    record->eviction_list_iterator
        = list.records.insert(list.records.end(), record);
    if (record->data.ptr)
        list.total_size += record->data.ptr->deep_size();
}

static void
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

immutable_cache_job_interface*
get_job_interface(immutable_cache_record* record)
{
    std::scoped_lock<std::mutex> lock(record->owner_cache->mutex);
    return record->job.get();
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
    immutable_cache& cache, id_interface const& key)
{
    if (!key_.matches(key))
    {
        this->reset();
        this->acquire(cache, key);
    }
}

void
untyped_immutable_cache_ptr::acquire(
    immutable_cache& cache, id_interface const& key)
{
    r_ = acquire_cache_record(cache, key);
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

} // namespace cradle