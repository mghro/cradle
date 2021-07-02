#ifndef CRADLE_BACKGROUND_REQUESTS_H
#define CRADLE_BACKGROUND_REQUESTS_H

#include <memory>

#include <cradle/core/id.h>
#include <cradle/utilities/functional.h>

namespace cradle {

struct untyped_request_interface
{
    virtual bool
    is_resolved() const = 0;

    virtual id_interface const&
    value_id() const = 0;
};

template<class Value>
struct request_interface : untyped_request_interface
{
    virtual void
    dispatch(function_view<void(Value value)> callback) const = 0;
};

template<class T>
using request_ptr = std::unique_ptr<request_interface<T>>;

namespace detail {

struct request_resolution_system;

}

struct request_resolution_system
{
    ~request_resolution_system();

    std::unique_ptr<detail::request_resolution_system> impl_;
};

template<class Value>
struct value_request : request_interface<Value>
{
    value_request(Value value) : value_(std::move(value))
    {
    }

    bool
    is_resolved() const override
    {
        return true;
    }

    id_interface const&
    value_id() const override
    {
        id_ = make_id_by_reference(value_);
        return id_;
    }

    void
    dispatch(function_view<void(Value value)> callback) const override
    {
        callback(value_);
    }

 private:
    Value value_;
    simple_id_by_reference<Value> mutable id_;
};

namespace rq {

template<class Value>
value_request<Value>
value(Value value)
{
    return value_request<Value>(std::move(value));
}

} // namespace rq

} // namespace cradle

#endif
