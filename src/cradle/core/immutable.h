#ifndef CRADLE_CORE_IMMUTABLE_HPP
#define CRADLE_CORE_IMMUTABLE_HPP

#include <cradle/core/api_types.hpp>
#include <cradle/core/dynamic.hpp>
#include <cradle/core/hash.hpp>
#include <cradle/core/type_info.h>
#include <cradle/core/utilities.hpp>

namespace cradle {

// immutable<T> holds (by shared_ptr) an immutable value of type T.
// immutable also provides the ability to efficiently store values in a
// dynamically-typed way. Any immutable<T> can be cast to and from
// untyped_immutable.

static inline bool
is_initialized(untyped_immutable const& x)
{
    return x.ptr ? true : false;
}

static inline untyped_immutable_value const*
get_value_pointer(untyped_immutable const& x)
{
    return x.ptr.get();
}

static inline void
reset(untyped_immutable& x)
{
    x.ptr.reset();
}

template<class T>
struct immutable_value : untyped_immutable_value
{
    T value;
    api_type_info
    type_info() const
    {
        return get_type_info(T());
    }
    size_t
    deep_size() const
    {
        return deep_sizeof(this->value);
    }
    size_t
    hash() const
    {
        return invoke_hash(this->value);
    }
    cradle::dynamic
    as_dynamic() const
    {
        return to_dynamic(this->value);
    }
    bool
    equals(untyped_immutable_value const* other) const
    {
        auto const* typed_other
            = dynamic_cast<immutable_value<T> const*>(other);
        return typed_other && this->value == typed_other->value;
    }
};

template<class T>
struct immutable
{
    std::shared_ptr<immutable_value<T>> ptr;
};

template<class T>
bool
is_initialized(immutable<T> const& x)
{
    return x.ptr ? true : false;
}

template<class T>
T const&
operator*(immutable<T> const& x)
{
    return x.ptr->value;
}

template<class T>
void
reset(immutable<T>& x)
{
    x.ptr.reset();
}

template<class T>
void
initialize(immutable<T>& x, T const& value)
{
    x.ptr.reset(new immutable_value<T>);
    x.ptr->value = value;
}

template<class T>
immutable<T>
make_immutable(T const& value)
{
    immutable<T> x;
    initialize(x, value);
    return x;
}

template<class T>
size_t
hash_value(immutable<T> const& x)
{
    return is_initialized(x) ? cradle::invoke_hash(*x) : 0;
}

template<class T>
void
swap_in(immutable<T>& x, T& value)
{
    x.ptr.reset(new immutable_value<T>);
    swap(x.ptr->value, value);
}

template<class T>
struct type_info_query<immutable<T>> : type_info_query<T>
{
};

template<class T>
size_t
deep_sizeof(immutable<T> const& x)
{
    return x.ptr->deep_size();
}

template<class T>
void
swap(immutable<T>& a, immutable<T>& b)
{
    swap(a.ptr, b.ptr);
}

template<class T>
bool
operator==(immutable<T> const& a, immutable<T> const& b)
{
    // First test if the two immutables are actually pointing to the same
    // thing, which avoids having to do a deep comparison for this case.
    return a.ptr == b.ptr || (a.ptr ? (b.ptr && *a == *b) : !b.ptr);
}
template<class T>
bool
operator!=(immutable<T> const& a, immutable<T> const& b)
{
    return !(a == b);
}
template<class T>
bool
operator<(immutable<T> const& a, immutable<T> const& b)
{
    return b.ptr && (a.ptr ? *a < *b : true);
}

template<class T>
void
from_dynamic(immutable<T>* x, cradle::dynamic const& v)
{
    using cradle::from_dynamic;
    x->ptr.reset(new immutable_value<T>);
    from_dynamic(&x->ptr->value, v);
}
template<class T>
void
to_dynamic(cradle::dynamic* v, immutable<T> const& x)
{
    using cradle::to_dynamic;
    if (x.ptr)
        to_dynamic(v, *x);
    else
        to_dynamic(v, T());
}

template<class T>
std::ostream&
operator<<(std::ostream& s, immutable<T> const& x)
{
    if (x.ptr)
        s << *x;
    else
        s << T();
    return s;
}

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(api_type_mismatch)
CRADLE_DEFINE_ERROR_INFO(api_type_info, expected_api_type)
CRADLE_DEFINE_ERROR_INFO(api_type_info, actual_api_type)

// Cast an untyped_immutable to a typed one.
template<class T>
immutable<T>
cast_immutable(untyped_immutable const& untyped)
{
    immutable<T> typed;
    if (untyped.ptr)
    {
        typed.ptr = std::dynamic_pointer_cast<immutable_value<T>>(untyped.ptr);
        if (!typed.ptr)
        {
            CRADLE_THROW(
                api_type_mismatch()
                << get_type_info<T>() << untyped.ptr->type_info());
        }
    }
    return typed;
}

// Cast an untyped_immutable to a typed one.
template<class T>
void
from_immutable(T* value, untyped_immutable const& untyped)
{
    *value = *cast_immutable<T>(untyped);
}

// This is a lower level form of cast that works directly on the value
// pointers.
template<class T>
void
cast_immutable_value(
    T const** typed_value, untyped_immutable_value const* untyped)
{
    immutable_value<T> const* typed
        = dynamic_cast<immutable_value<T> const*>(untyped);
    if (!typed)
    {
        CRADLE_THROW(
            api_type_mismatch() << get_type_info<T>() << untyped->type_info());
    }
    *typed_value = &typed->value;
}

// Erase the compile-time type information associated with the given immutable
// to produce an untyped_immutable.
template<class T>
untyped_immutable
erase_type(immutable<T> const& typed)
{
    untyped_immutable untyped;
    untyped.ptr = std::static_pointer_cast<untyped_immutable_value>(typed.ptr);
    return untyped;
}

template<class T>
untyped_immutable
swap_in_and_erase_type(T& value)
{
    immutable<T> tmp;
    swap_in(tmp, value);
    return erase_type(tmp);
}

untyped_immutable
get_field(
    std::map<string, untyped_immutable> const& fields,
    string const& field_name);

} // namespace cradle

#endif
