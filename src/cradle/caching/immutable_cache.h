#ifndef CRADLE_CACHING_IMMUTABLE_CACHE_H
#define CRADLE_CACHING_IMMUTABLE_CACHE_H

#include <cradle/common.hpp>
#include <cradle/core/id.hpp>

// This file defines a small framework for caching immutable data in memory.
//
// The cache is designed with the following requirements in mind:
//
// - Cached data may be large.
//
// - Data may take time to retrieve or compute. This must be allowed to happen
//   concurrently as callers continue to access the cache.
//
// - Multiple callers may be interested in the same data.
//   - These calls may come concurrently from different threads.
//   - This should not result in duplicated data instances or effort.
//
// - Callers will be interested in individual data objects for some period of
//   time. (The API must allow them to indicate this duration by holding a
//   handle/pointer to data that they're interested in.)
//
// - It is useful to retain data that is no longer in use.
//   - But this must be subject to constraints on total memory used for this
//     effort (or the amount that remains available to the system).
//
// - A single cache instance should be able to store heterogenous data types
//   while preserving type safety for callers.
//
// - Efforts to retrieve or compute data may fail.
//
// - The cache must provide an inspection interface.
//
// Note that the cache is intentionally agnostic to the methods used to
// retrieve (or generate) data. It does track whether or not such operations
// are ongoing and provides a small interface for interacting with them, but
// that interface is intentionally minimalist. It's expected that additional
// capabilities will be provided externally for the user to interact with those
// jobs (to investigate failures, retry jobs, etc.).
//
// The keys to the cache are CRADLE IDs, which allows for efficient usage of
// heterogenous keys (without necessarily resorting to string
// conversion/hashes).

namespace cradle {

// This stores an optional progress value encoded as an integer so that it can
// be stored atomically.
struct encoded_optional_progress
{
    // Progress is encoded as an integer ranging from 0 to
    // `encoded_progress_max_value`.
    //
    // A negative value indicates that progress hasn't been reported.
    //
    int value = -1;
};
int constexpr encoded_progress_max_value = 1000;
inline encoded_optional_progress
encode_progress(float progress)
{
    return encoded_optional_progress{int(progress / 1000.f)};
}
inline void
reset(encoded_optional_progress& progress)
{
    progress.value = -1;
}
inline optional<float>
decode_progress(encoded_optional_progress progress)
{
    return progress.value < 0 ? none : some(float(progress.value) * 1000.f);
}

enum class immutable_cache_data_state
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

struct immutable_cache_data_status
{
    immutable_cache_data_state state = immutable_cache_data_state::LOADING;
    // Only valid if state is LOADING, but still optional even then.
    encoded_optional_progress progress;
};

enum class immutable_cache_job_state
{
    QUEUED,
    RUNNING,
    COMPLETED,
    FAILED
};

struct immutable_cache_job_status
{
    immutable_cache_job_state state = immutable_cache_job_state::QUEUED;
    // Only valid if state is RUNNING, but still optional even then.
    encoded_optional_progress progress;
};

struct immutable_cache_job_interface
{
    // If this is invoked before the job completes, the job should assume that
    // there is no longer any interest in its result and should cancel itself
    // if possible.
    virtual ~immutable_cache_job_interface()
    {
    }

    // Get the status of the job.
    virtual immutable_cache_job_status
    status() const = 0;
};

struct immutable_cache;
struct immutable_cache_record;

// immutable_cache_ptr represents one's interest in a particular immutable
// value. The value is assumed to be the result of performing some operation
// (with reproducible results). If there are already other parties interested
// in the result, the pointer will immediately pick up whatever progress has
// already been made in computing that result. Otherwise, the owner must create
// a new job to produce the result and associate it with the pointer.

// untyped_immutable_cache_ptr provides all of the functionality of
// immutable_cache_ptr without compile-time knowledge of the data type.
struct untyped_immutable_cache_ptr
{
    untyped_immutable_cache_ptr() : r_(nullptr)
    {
    }
    untyped_immutable_cache_ptr(
        immutable_cache& cache, id_interface const& key)
    {
        acquire(cache, key);
    }
    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr const& other)
    {
        copy(other);
    }

    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr const& other)
    {
        reset();
        copy(other);
        return *this;
    }

    void
    swap(untyped_immutable_cache_ptr& other);

    ~untyped_immutable_cache_ptr()
    {
        reset();
    }

    void
    reset();

    void
    reset(immutable_cache& cache, id_interface const& key);

    bool
    is_initialized() const
    {
        return r_ != nullptr;
    }

    immutable_cache_data_status const&
    status() const
    {
        return status_;
    }

    immutable_cache_data_state
    state() const
    {
        return status_.state;
    }
    bool
    is_loading() const
    {
        return this->state() == immutable_cache_data_state::LOADING;
    }
    bool
    is_ready() const
    {
        return this->state() == immutable_cache_data_state::READY;
    }
    bool
    is_failed() const
    {
        return this->state() == immutable_cache_data_state::FAILED;
    }

    optional<float>
    progress() const
    {
        return decode_progress(status_.progress);
    }

    // Everything below here should only be called if the pointer is
    // initialized...

    // Update this pointer's view of the underlying record's state.
    void
    update();

    id_interface const&
    key() const
    {
        return *key_;
    }

    immutable_cache_record*
    record() const
    {
        return r_;
    }

    // This provides access to the actual data.
    // Only use this if state is READY.
    untyped_immutable const&
    data() const
    {
        return data_;
    }

 private:
    void
    copy(untyped_immutable_cache_ptr const& other);

    void
    acquire(immutable_cache& cache, id_interface const& key);

    captured_id key_;

    immutable_cache_record* r_;
    immutable_cache_data_status status_;

    // This is a local copy of the data pointer. Actually acquiring this
    // pointer requires synchronization, but once it's acquired, it can be
    // used freely without synchronization.
    untyped_immutable data_;
};

inline void
swap(untyped_immutable_cache_ptr& a, untyped_immutable_cache_ptr& b)
{
    a.swap(b);
}

// immutable_cache_ptr<T> wraps untyped_immutable_cache_ptr to provide access
// to immutable cache data of a known type.
template<class T>
struct immutable_cache_ptr
{
    immutable_cache_ptr() : data_(nullptr)
    {
    }

    immutable_cache_ptr(untyped_immutable_cache_ptr& untyped)
        : untyped_(untyped)
    {
        refresh_typed();
    }

    immutable_cache_ptr(
        immutable_cache& cache, id_interface const& key, function_view<void()>)
    {
        reset(system, key);
    }

    void
    reset()
    {
        untyped_.reset();
        data_ = nullptr;
    }

    void
    reset(immutable_cache& cache, id_interface const& key)
    {
        untyped_.reset(system, key);
        refresh_typed();
    }

    bool
    is_initialized() const
    {
        return untyped_.is_initialized();
    }

    immutable_cache_data_status const&
    status() const
    {
        return untyped_.status();
    }

    immutable_cache_data_state
    state() const
    {
        return untyped_.state();
    }
    bool
    is_loading() const
    {
        return untyped_.is_loading();
    }
    bool
    is_ready() const
    {
        return untyped_.is_ready();
    }
    bool
    is_failed() const
    {
        return untyped_.is_failed();
    }

    optional<float> const&
    progress() const
    {
        return untyped_.progress();
    }

    // Everything below here should only be called if the pointer is
    // initialized...

    // Update this pointer's view of the underlying record's state.
    void
    update()
    {
        if (!this->is_ready())
        {
            untyped_.update();
            this->refresh_typed();
        }
    }

    id_interface const&
    key() const
    {
        return untyped_.key();
    }

    // Access the underlying untyped pointer.
    untyped_immutable_cache_ptr const&
    untyped() const
    {
        return untyped_;
    }
    untyped_immutable_cache_ptr&
    untyped()
    {
        return untyped_;
    }

    // When the underlying untyped pointer changes, this must be called to
    // update the typed data in response to those changes.
    void
    refresh_typed()
    {
        if (this->state() == background_data_state::READY)
            cast_immutable_value(&data_, untyped_.data().ptr.get());
        else
            data_ = nullptr;
    }

    // These provide access to the actual data.
    // Only use these if state is READY...
    T const&
    operator*() const
    {
        return *data_;
    }
    T const*
    operator->() const
    {
        return data_;
    }

    void
    swap(immutable_cache_ptr& other)
    {
        using std::swap;
        swap(untyped_, other.untyped_);
        swap(data_, other.data_);
    }

 private:
    untyped_immutable_cache_ptr untyped_;

    // typed pointer to the data
    T const* data_;
};

template<class T>
void
swap(immutable_cache_ptr<T>& a, immutable_cache_ptr<T>& b)
{
    a.swap(b);
}

} // namespace cradle

#endif
