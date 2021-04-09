#ifndef CRADLE_CACHING_IMMUTABLE_CONSUMPTION_H
#define CRADLE_CACHING_IMMUTABLE_CONSUMPTION_H

#include <cradle/background/encoded_progress.h>
#include <cradle/caching/immutable/cache.hpp>
#include <cradle/core.h>
#include <cradle/core/id.h>

namespace cradle {

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

// immutable_cache_ptr represents one's interest in a particular immutable
// value. The value is assumed to be the result of performing some operation
// (with reproducible results). If there are already other parties interested
// in the result, the pointer will immediately pick up whatever progress has
// already been made in computing that result. Otherwise, the owner must create
// a new job to produce the result and associate it with the pointer.

namespace detail {

struct immutable_cache_record;

// untyped_immutable_cache_ptr provides all of the functionality of
// immutable_cache_ptr without compile-time knowledge of the data type.
struct untyped_immutable_cache_ptr
{
    untyped_immutable_cache_ptr() : r_(nullptr)
    {
    }
    untyped_immutable_cache_ptr(
        cradle::immutable_cache& cache, id_interface const& key)
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
    reset(cradle::immutable_cache& cache, id_interface const& key);

    bool
    is_initialized() const
    {
        return r_ != nullptr;
    }

    // Everything below here should only be called if the pointer is
    // initialized...

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

    // Update this pointer's view of the underlying 4's state.
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
    acquire(cradle::immutable_cache& cache, id_interface const& key);

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

} // namespace detail

// immutable_cache_ptr<T> wraps untyped_immutable_cache_ptr to provide access
// to immutable cache data of a known type.
template<class T>
struct immutable_cache_ptr
{
    immutable_cache_ptr() : data_(nullptr)
    {
    }

    immutable_cache_ptr(detail::untyped_immutable_cache_ptr& untyped)
        : untyped_(untyped)
    {
        refresh_typed();
    }

    immutable_cache_ptr(
        cradle::immutable_cache& cache, id_interface const& key)
    {
        reset(cache, key);
    }

    void
    reset()
    {
        untyped_.reset();
        data_ = nullptr;
    }

    void
    reset(cradle::immutable_cache& cache, id_interface const& key)
    {
        untyped_.reset(cache, key);
        refresh_typed();
    }

    bool
    is_initialized() const
    {
        return untyped_.is_initialized();
    }

    // Everything below here should only be called if the pointer is
    // initialized...

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
    detail::untyped_immutable_cache_ptr const&
    untyped() const
    {
        return untyped_;
    }
    detail::untyped_immutable_cache_ptr&
    untyped()
    {
        return untyped_;
    }

    // When the underlying untyped pointer changes, this must be called to
    // update the typed data in response to those changes.
    void
    refresh_typed()
    {
        if (this->state() == immutable_cache_data_state::READY)
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
    detail::untyped_immutable_cache_ptr untyped_;

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
