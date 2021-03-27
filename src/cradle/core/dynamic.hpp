#ifndef CRADLE_CORE_VALUE_HPP
#define CRADLE_CORE_VALUE_HPP

#include <initializer_list>
#include <list>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cradle/core/exception.hpp>
#include <cradle/core/type_definitions.hpp>

namespace cradle {

// DYNAMIC VALUES - Dynamic values are values whose structure is determined at
// run-time rather than compile time.

std::ostream&
operator<<(std::ostream& s, value_type t);

// Check that two value types match.
void
check_type(value_type expected, value_type actual);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(type_mismatch)
CRADLE_DEFINE_ERROR_INFO(value_type, expected_value_type)
CRADLE_DEFINE_ERROR_INFO(value_type, actual_value_type)

// Get the value_type value for a C++ type.
template<class T>
struct value_type_of
{
};
template<>
struct value_type_of<nil_t>
{
    static value_type const value = value_type::NIL;
};
template<>
struct value_type_of<bool>
{
    static value_type const value = value_type::BOOLEAN;
};
template<>
struct value_type_of<integer>
{
    static value_type const value = value_type::INTEGER;
};
template<>
struct value_type_of<double>
{
    static value_type const value = value_type::FLOAT;
};
template<>
struct value_type_of<string>
{
    static value_type const value = value_type::STRING;
};
template<>
struct value_type_of<blob>
{
    static value_type const value = value_type::BLOB;
};
template<>
struct value_type_of<boost::posix_time::ptime>
{
    static value_type const value = value_type::DATETIME;
};
template<>
struct value_type_of<dynamic_array>
{
    static value_type const value = value_type::ARRAY;
};
template<>
struct value_type_of<dynamic_map>
{
    static value_type const value = value_type::MAP;
};

// MAPS

// This queries a map for a field with a key matching the given string.
// If the field is not present in the map, an exception is thrown.
dynamic const&
get_field(dynamic_map const& r, string const& field);
// non-const version
dynamic&
get_field(dynamic_map& r, string const& field);

CRADLE_DEFINE_EXCEPTION(missing_field)
CRADLE_DEFINE_ERROR_INFO(string, field_name)

// This is the same as above, but its return value indicates whether or not
// the field is in the map.
bool
get_field(dynamic const** v, dynamic_map const& r, string const& field);
// non-const version
bool
get_field(dynamic** v, dynamic_map& r, string const& field);

// Given a dynamic_map that's meant to represent a union value, this checks
// that the map contains only one value and returns its key.
dynamic const&
get_union_tag(dynamic_map const& map);

CRADLE_DEFINE_EXCEPTION(multifield_union)

// When an error occurs in the processing of a dynamic value, this provides the
// path to the location within the value where the error occurred.
CRADLE_DEFINE_ERROR_INFO(std::list<dynamic>, dynamic_value_path)

// Given an exception :e, this will add :path_element to the beginning of the
// dynamic_value_path info associated with :e. If there is currently no path
// info associated with :e, a path containing only :p is associated with it.
void
add_dynamic_path_element(boost::exception& e, dynamic const& path_element);

// VALUES

// Cast a dynamic value to one of the base types.
template<class T>
T const&
cast(dynamic const& v)
{
    check_type(value_type_of<T>::value, v.type());
    return std::any_cast<T const&>(v.contents());
}
// Same, but with a non-const reference.
template<class T>
T&
cast(dynamic& v)
{
    check_type(value_type_of<T>::value, v.type());
    return std::any_cast<T&>(v.contents());
}
// Same, but with move semantics.
template<class T>
T&&
cast(dynamic&& v)
{
    check_type(value_type_of<T>::value, v.type());
    return std::any_cast<T&&>(std::move(v).contents());
}

std::ostream&
operator<<(std::ostream& os, dynamic const& v);

std::ostream&
operator<<(std::ostream& os, std::list<dynamic> const& v);

// The following implements the CRADLE Regular type interface, plus some
// additional comparison operators.

struct api_type_info;

template<>
struct type_info_query<dynamic>
{
    static void
    get(api_type_info* info);
};

size_t
deep_sizeof(dynamic const& v);

void
swap(dynamic& a, dynamic& b);

bool
operator==(dynamic const& a, dynamic const& b);
bool
operator!=(dynamic const& a, dynamic const& b);
bool
operator<(dynamic const& a, dynamic const& b);
bool
operator<=(dynamic const& a, dynamic const& b);
bool
operator>(dynamic const& a, dynamic const& b);
bool
operator>=(dynamic const& a, dynamic const& b);

inline void
to_dynamic(dynamic* v, dynamic const& x)
{
    *v = x;
}
inline void
from_dynamic(dynamic* x, dynamic const& v)
{
    *x = v;
}

size_t
hash_value(dynamic const& x);

// All regular CRADLE types provide to_dynamic(&v, x) and from_dynamic(&x, v).
// The following are alternate, often more convenient forms.
template<class T>
dynamic
to_dynamic(T const& x)
{
    dynamic v;
    to_dynamic(&v, x);
    return v;
}
template<class T>
T
from_dynamic(dynamic const& v)
{
    T x;
    from_dynamic(&x, v);
    return x;
}

// Apply the functor fn to the value v.
// fn must have the function call operator overloaded for all supported
// types (including nil). If it doesn't, you'll get a compile-time error.
// fn is passed as a non-const reference so that it can accumulate results.
template<class Fn>
auto
apply_to_dynamic(Fn&& fn, dynamic const& v)
{
    switch (v.type())
    {
        case value_type::NIL:
        default: // All cases are covered, so this is just to avoid warnings.
            return fn(nil);
        case value_type::BOOLEAN:
            return fn(cast<bool>(v));
        case value_type::INTEGER:
            return fn(cast<integer>(v));
        case value_type::FLOAT:
            return fn(cast<double>(v));
        case value_type::STRING:
            return fn(cast<string>(v));
        case value_type::BLOB:
            return fn(cast<blob>(v));
        case value_type::DATETIME:
            return fn(cast<boost::posix_time::ptime>(v));
        case value_type::ARRAY:
            return fn(cast<dynamic_array>(v));
        case value_type::MAP:
            return fn(cast<dynamic_map>(v));
    }
}

// Apply the functor fn to two values of the same type.
// If a and b are not the same type, this throws a type_mismatch exception.
template<class Fn>
auto
apply_to_dynamic_pair(Fn&& fn, dynamic const& a, dynamic const& b)
{
    check_type(a.type(), b.type());
    switch (a.type())
    {
        case value_type::NIL:
        default: // All cases are covered, so this is just to avoid warnings.
            return fn(nil, nil);
        case value_type::BOOLEAN:
            return fn(cast<bool>(a), cast<bool>(b));
        case value_type::INTEGER:
            return fn(cast<integer>(a), cast<integer>(b));
        case value_type::FLOAT:
            return fn(cast<double>(a), cast<double>(b));
        case value_type::STRING:
            return fn(cast<string>(a), cast<string>(b));
        case value_type::BLOB:
            return fn(cast<blob>(a), cast<blob>(b));
        case value_type::DATETIME:
            return fn(
                cast<boost::posix_time::ptime>(a),
                cast<boost::posix_time::ptime>(b));
        case value_type::ARRAY:
            return fn(cast<dynamic_array>(a), cast<dynamic_array>(b));
        case value_type::MAP:
            return fn(cast<dynamic_map>(a), cast<dynamic_map>(b));
    }
}

struct api_named_type_reference;
struct api_type_info;

// Coerce a dynamic value to match the given type.
// This only applies very gentle coercions (e.g., lossless numeric casts).
// :look_up_named_type must be implemented by the caller as a means for the
// algorithm to look up named types.
dynamic
coerce_value(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    dynamic value);

// This is a generic function for reading a field from a dynamic_map.
// It exists primarily so that omissible types can override it.
template<class Field>
void
read_field_from_record(
    Field* field_value, dynamic_map const& record, string const& field_name)
{
    auto dynamic_field_value = get_field(record, field_name);
    try
    {
        from_dynamic(field_value, dynamic_field_value);
    }
    catch (boost::exception& e)
    {
        cradle::add_dynamic_path_element(e, field_name);
        throw;
    }
}

// This is a generic function for writing a field to a dynamic_map.
// It exists primarily so that omissible types can override it.
template<class Field>
void
write_field_to_record(
    dynamic_map& record, std::string field_name, Field field_value)
{
    to_dynamic(
        &record[dynamic(std::move(field_name))], std::move(field_value));
}

} // namespace cradle

#endif
