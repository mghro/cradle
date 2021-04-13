#ifndef CRADLE_CACHING_IMMUTABLE_CONSUMPTION_H
#define CRADLE_CACHING_IMMUTABLE_CONSUMPTION_H

#include <cradle/background/encoded_progress.h>
#include <cradle/background/job.h>
#include <cradle/core.h>
#include <cradle/utilities/functional.h>

#include <cradle/caching/immutable/cache.hpp>

namespace cradle {

struct immutable_cache;

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
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job)
    {
        acquire(cache, key, create_job);
    }
    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr const& other)
    {
        copy(other);
    }
    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr&& other)
    {
        move_in(std::move(other));
    }

    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr const& other)
    {
        reset();
        copy(other);
        return *this;
    }
    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr&& other)
    {
        reset();
        move_in(std::move(other));
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
    reset(
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job);

    bool
    is_initialized() const
    {
        return r_ != nullptr;
    }

    // Everything below here should only be called if the pointer is
    // initialized...

    immutable_cache_entry_status
    status() const
    {
        return immutable_cache_entry_status{this->state(), this->progress()};
    }

    immutable_cache_entry_state
    state() const
    {
        return state_;
    }
    bool
    is_loading() const
    {
        return this->state() == immutable_cache_entry_state::LOADING;
    }
    bool
    is_ready() const
    {
        return this->state() == immutable_cache_entry_state::READY;
    }
    bool
    is_failed() const
    {
        return this->state() == immutable_cache_entry_state::FAILED;
    }

    optional<float>
    progress() const
    {
        return decode_progress(progress_);
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
    move_in(untyped_immutable_cache_ptr&& other);

    void
    acquire(
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job);

    // the key of the entry that this pointer points to
    captured_id key_;

    // the internal cache record for the entry
    immutable_cache_record* r_;

    // this pointer's view of the status of the entry
    immutable_cache_entry_state state_;
    encoded_optional_progress progress_;

    // a local copy of the data pointer - Actually acquiring this pointer
    // requires synchronization, but once it's acquired, it can be used freely
    // without synchronization.
    untyped_immutable data_;
};

inline void
swap(untyped_immutable_cache_ptr& a, untyped_immutable_cache_ptr& b)
{
    a.swap(b);
}

} // namespace detail

// immutable_cache_ptr<T> represents one's interest in a particular immutable
// value (of type T). The value is assumed to be the result of performing some
// operation (with reproducible results). If there are already other parties
// interested in the result, the pointer will immediately pick up whatever
// progress has already been made in computing that result.
//
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
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job)
    {
        reset(cache, key, create_job);
    }

    void
    reset()
    {
        untyped_.reset();
        data_ = nullptr;
    }

    void
    reset(
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job)
    {
        untyped_.reset(cache, key, create_job);
        refresh_typed();
    }

    bool
    is_initialized() const
    {
        return untyped_.is_initialized();
    }

    // Everything below here should only be called if the pointer is
    // initialized...

    immutable_cache_entry_status const&
    status() const
    {
        return untyped_.status();
    }

    immutable_cache_entry_state
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

    optional<float>
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
        if (this->state() == immutable_cache_entry_state::READY)
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
