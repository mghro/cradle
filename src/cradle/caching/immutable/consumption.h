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

}

// immutable_cache_entry_watcher is the interface that must be implemented by
// objects that want to watch for results on an immutable cache entry.
struct immutable_cache_entry_watcher
{
    virtual void
    on_progress(float progress)
    {
    }

    virtual void
    on_failure()
    {
    }

    virtual void
    on_ready(untyped_immutable value)
        = 0;
};

// immutable_cache_entry_handle provides ownership of an immutable cache
// entry.
struct immutable_cache_entry_handle
{
    immutable_cache_entry_handle()
    {
    }
    immutable_cache_entry_handle(
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job,
        std::shared_ptr<immutable_cache_entry_watcher> watcher
        = std::shared_ptr<immutable_cache_entry_watcher>())
    {
        acquire(cache, key, create_job, std::move(watcher));
    }
    immutable_cache_entry_handle(immutable_cache_entry_handle const& other)
    {
        copy(other);
    }
    immutable_cache_entry_handle(immutable_cache_entry_handle&& other)
    {
        move_in(std::move(other));
    }

    immutable_cache_entry_handle&
    operator=(immutable_cache_entry_handle const& other)
    {
        reset();
        copy(other);
        return *this;
    }
    immutable_cache_entry_handle&
    operator=(immutable_cache_entry_handle&& other)
    {
        reset();
        move_in(std::move(other));
        return *this;
    }

    void
    swap(immutable_cache_entry_handle& other);

    ~immutable_cache_entry_handle()
    {
        reset();
    }

    void
    reset();

    void
    reset(
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job,
        std::shared_ptr<immutable_cache_entry_watcher> watcher
        = std::shared_ptr<immutable_cache_entry_watcher>());

    bool
    is_initialized() const
    {
        return record_ != nullptr;
    }

    id_interface const&
    key() const
    {
        return *key_;
    }

    detail::immutable_cache_record*
    record() const
    {
        return record_;
    }

 private:
    void
    copy(immutable_cache_entry_handle const& other);

    void
    move_in(immutable_cache_entry_handle&& other);

    void
    acquire(
        cradle::immutable_cache& cache,
        id_interface const& key,
        function_view<background_job_controller()> const& create_job,
        std::shared_ptr<immutable_cache_entry_watcher> watcher);

    // the key of the entry
    captured_id key_;

    // the internal cache record for the entry
    detail::immutable_cache_record* record_ = nullptr;

    // the watcher (if any) associated with this handle
    std::shared_ptr<immutable_cache_entry_watcher> watcher_;
};

namespace detail {

// untyped_immutable_cache_ptr provides all of the functionality of
// immutable_cache_ptr without compile-time knowledge of the data type.
struct untyped_immutable_cache_ptr
{
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
        return handle_.is_initialized();
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

    // Update this pointer's view of the underlying entry's state.
    void
    update();

    id_interface const&
    key() const
    {
        return handle_.key();
    }

    immutable_cache_record*
    record() const
    {
        return handle_.record();
    }

    // This provides access to the actual data.
    // Only use this if state is READY.
    untyped_immutable const&
    data() const
    {
        return data_;
    }

 private:
    immutable_cache_entry_handle handle_;

    // this pointer's view of the status of the entry
    immutable_cache_entry_state state_ = immutable_cache_entry_state::LOADING;
    encoded_optional_progress progress_;

    // a local copy of the data pointer - Actually acquiring this pointer
    // requires synchronization, but once it's acquired, it can be used freely
    // without synchronization.
    untyped_immutable data_;
};

} // namespace detail

// immutable_cache_ptr<T> represents one's interest in a particular immutable
// value (of type T). The value is assumed to be the result of performing some
// operation (with reproducible results). If there are already other parties
// interested in the result, the pointer will immediately pick up whatever
// progress has already been made in computing that result.
//
// This is a polling-based approach to observing a cache value.
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

 private:
    detail::untyped_immutable_cache_ptr untyped_;

    // typed pointer to the data
    T const* data_;
};

} // namespace cradle

#endif
