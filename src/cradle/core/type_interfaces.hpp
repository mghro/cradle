#ifndef CRADLE_CORE_TYPE_INTERFACES_HPP
#define CRADLE_CORE_TYPE_INTERFACES_HPP

#include <map>
#include <vector>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cradle/core/api_types.hpp>
#include <cradle/core/dynamic.hpp>
#include <cradle/core/exception.hpp>
#include <cradle/core/hash.hpp>
#include <cradle/core/type_info.h>

// This file provides implementations of the CRADLE Regular interface
// for all the core CRADLE types.

namespace cradle {

// NIL

inline bool operator==(nil_t, nil_t)
{
    return true;
}

inline bool operator!=(nil_t, nil_t)
{
    return false;
}

inline bool operator<(nil_t, nil_t)
{
    return false;
}

template<>
struct type_info_query<nil_t>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_nil_type(api_nil_type());
    }
};

inline size_t deep_sizeof(nil_t)
{
    return 0;
}

struct dynamic;

// Note that we don't have to do anything here because callers of to_dynamic
// are required to provide a default-constructed dynamic, which is already nil.
inline void
to_dynamic(dynamic*, nil_t)
{
}

inline void
from_dynamic(nil_t*, dynamic const&)
{
}

inline size_t hash_value(nil_t)
{
    return 0;
}

// BOOL

template<>
struct type_info_query<bool>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_boolean_type(api_boolean_type());
    }
};

inline size_t
deep_sizeof(bool)
{
    return sizeof(bool);
}

void
to_dynamic(dynamic* v, bool x);

void
from_dynamic(bool* x, dynamic const& v);

// INTEGERS AND FLOATS

#define CRADLE_DECLARE_NUMBER_INTERFACE(T)                                    \
    void to_dynamic(dynamic* v, T x);                                         \
                                                                              \
    void from_dynamic(T* x, dynamic const& v);                                \
                                                                              \
    inline size_t deep_sizeof(T)                                              \
    {                                                                         \
        return sizeof(T);                                                     \
    }

#define CRADLE_DECLARE_INTEGER_INTERFACE(T)                                   \
    template<>                                                                \
    struct type_info_query<T>                                                 \
    {                                                                         \
        static void                                                           \
        get(api_type_info* info)                                              \
        {                                                                     \
            *info = make_api_type_info_with_integer_type(api_integer_type()); \
        }                                                                     \
    };                                                                        \
                                                                              \
    integer to_integer(T x);                                                  \
                                                                              \
    void from_integer(T* x, integer n);                                       \
                                                                              \
    CRADLE_DECLARE_NUMBER_INTERFACE(T)

CRADLE_DECLARE_INTEGER_INTERFACE(signed char)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned char)
CRADLE_DECLARE_INTEGER_INTERFACE(signed short)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned short)
CRADLE_DECLARE_INTEGER_INTERFACE(signed int)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned int)
CRADLE_DECLARE_INTEGER_INTERFACE(signed long)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned long)
CRADLE_DECLARE_INTEGER_INTERFACE(signed long long)
CRADLE_DECLARE_INTEGER_INTERFACE(unsigned long long)

#define CRADLE_DECLARE_FLOAT_INTERFACE(T)                                     \
    template<>                                                                \
    struct type_info_query<T>                                                 \
    {                                                                         \
        static void                                                           \
        get(api_type_info* info)                                              \
        {                                                                     \
            *info = make_api_type_info_with_float_type(api_float_type());     \
        }                                                                     \
    };                                                                        \
    CRADLE_DECLARE_NUMBER_INTERFACE(T)

CRADLE_DECLARE_FLOAT_INTERFACE(float)
CRADLE_DECLARE_FLOAT_INTERFACE(double)

// STRING

template<>
struct type_info_query<string>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_string_type(api_string_type());
    }
};

inline size_t
deep_sizeof(string const& x)
{
    return sizeof(string) + sizeof(char) * x.length();
}

void
to_dynamic(dynamic* v, string const& x);

void
from_dynamic(string* x, dynamic const& v);

// DATE

using boost::gregorian::date;

// Get the preferred CRADLE string representation of a date (YYYY-MM-DD).
string
to_string(date const& d);

void
to_dynamic(dynamic* v, date const& x);

void
from_dynamic(date* x, dynamic const& v);

template<>
struct type_info_query<date>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_string_type(api_string_type());
    }
};

inline size_t deep_sizeof(date)
{
    return sizeof(date);
}

} // namespace cradle

namespace boost {
namespace gregorian {

inline size_t
hash_value(date const& x)
{
    return cradle::invoke_hash(cradle::to_string(x));
}

} // namespace gregorian
} // namespace boost

namespace cradle {

// PTIME

using boost::posix_time::ptime;

// Get the preferred user-readable string representation of a ptime.
string
to_string(ptime const& t);

// Get the preferred representation for encoding a ptime as a string.
// (This preserves milliseconds.)
string
to_value_string(ptime const& t);

// Parse a ptime from a value string.
ptime
parse_ptime(std::string const& s);

void
to_dynamic(dynamic* v, ptime const& x);

void
from_dynamic(ptime* x, dynamic const& v);

template<>
struct type_info_query<ptime>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_datetime_type(api_datetime_type());
    }
};

inline size_t deep_sizeof(ptime)
{
    return sizeof(ptime);
}

} // namespace cradle

namespace boost {
namespace posix_time {

inline size_t
hash_value(ptime const& x)
{
    return cradle::invoke_hash(cradle::to_string(x));
}

} // namespace posix_time
} // namespace boost

namespace cradle {

// BLOB

bool
operator==(blob const& a, blob const& b);

inline bool
operator!=(blob const& a, blob const& b)
{
    return !(a == b);
}

bool
operator<(blob const& a, blob const& b);

template<>
struct type_info_query<blob>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_blob_type(api_blob_type());
    }
};

inline size_t
deep_sizeof(blob const& b)
{
    // This ignores the size of the ownership holder, but that's not a big
    // deal.
    return sizeof(blob) + b.size;
}

struct dynamic;

void
to_dynamic(dynamic* v, blob const& x);

void
from_dynamic(blob* x, dynamic const& v);

size_t
hash_value(blob const& x);

// Make a blob that holds the contents of the given string.
blob
make_string_blob(string s);

// STD::VECTOR

template<class T>
void
to_dynamic(dynamic* v, std::vector<T> const& x)
{
    dynamic_array array;
    size_t n_elements = x.size();
    array.resize(n_elements);
    for (size_t i = 0; i != n_elements; ++i)
    {
        to_dynamic(&array[i], x[i]);
    }
    *v = std::move(array);
}

template<class T>
void
from_dynamic(std::vector<T>* x, dynamic const& v)
{
    // Certain ways of encoding values (e.g., JSON) have the same
    // representation for empty arrays and empty maps, so if we encounter an
    // empty map here, we should treat it as an empty array.
    if (v.type() == value_type::MAP && cast<dynamic_map>(v).empty())
    {
        // *x is already empty because it's default-constructed.
        return;
    }

    dynamic_array const& array = cast<dynamic_array>(v);
    size_t n_elements = array.size();
    x->resize(n_elements);
    for (size_t i = 0; i != n_elements; ++i)
    {
        try
        {
            from_dynamic(&(*x)[i], array[i]);
        }
        catch (boost::exception& e)
        {
            add_dynamic_path_element(e, integer(i));
            throw;
        }
    }
}

template<class T>
struct type_info_query<std::vector<T>>
{
    static void
    get(api_type_info* info)
    {
        api_array_info array_info;
        array_info.element_schema = get_type_info<T>();
        array_info.size = none;
        *info = make_api_type_info_with_array_type(array_info);
    }
};

template<class T>
size_t
deep_sizeof(std::vector<T> const& x)
{
    size_t size = sizeof(std::vector<T>);
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

// STD::ARRAY

template<class T, size_t N>
void
to_dynamic(dynamic* v, std::array<T, N> const& x)
{
    dynamic_array l;
    l.resize(N);
    for (size_t i = 0; i != N; ++i)
    {
        to_dynamic(&l[i], x[i]);
    }
    *v = std::move(l);
}

template<class T, size_t N>
void
from_dynamic(std::array<T, N>* x, dynamic const& v)
{
    if (N == 0)
    {
        // Certain ways of encoding values (e.g., JSON) have the same
        // representation for empty arrays and empty maps, so if we encounter
        // an empty map here, we should treat it as an empty array.
        if (v.type() == value_type::MAP && cast<dynamic_map>(v).empty())
        {
            return;
        }
    }

    dynamic_array const& l = cast<dynamic_array>(v);
    check_array_size(N, l.size());
    for (size_t i = 0; i != N; ++i)
    {
        try
        {
            from_dynamic(&(*x)[i], l[i]);
        }
        catch (boost::exception& e)
        {
            add_dynamic_path_element(e, integer(i));
            throw;
        }
    }
}

template<class T, size_t N>
struct type_info_query<std::array<T, N>>
{
    static void
    get(api_type_info* info)
    {
        api_array_info array_info;
        array_info.element_schema = get_type_info<T>();
        array_info.size = some(N);
        *info = make_api_type_info_with_array_type(array_info);
    }
};

template<class T, size_t N>
size_t
deep_sizeof(std::array<T, N> const& x)
{
    size_t size = 0;
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

// STD::MAP

template<class Key, class Value>
void
to_dynamic(dynamic* v, std::map<Key, Value> const& x)
{
    dynamic_map map;
    for (auto const& i : x)
        to_dynamic(&map[to_dynamic(i.first)], i.second);
    *v = std::move(map);
}

template<class Key, class Value>
void
from_dynamic(std::map<Key, Value>* x, dynamic const& v)
{
    // Certain ways of encoding values (e.g., JSON) have the same
    // representation for empty arrays and empty maps, so if we encounter an
    // empty array here, we should treat it as an empty map.
    if (v.type() == value_type::ARRAY && cast<dynamic_array>(v).empty())
    {
        // *x is already empty because it's default-constructed.
        return;
    }

    dynamic_map const& map = cast<dynamic_map>(v);
    for (auto const& i : map)
    {
        try
        {
            from_dynamic(&(*x)[from_dynamic<Key>(i.first)], i.second);
        }
        catch (boost::exception& e)
        {
            add_dynamic_path_element(e, i.first);
            throw;
        }
    }
}

template<class Key, class Value>
struct type_info_query<std::map<Key, Value>>
{
    static void
    get(api_type_info* info)
    {
        api_map_info map_info;
        map_info.key_schema = get_type_info<Key>();
        map_info.value_schema = get_type_info<Value>();
        *info = make_api_type_info_with_map_type(map_info);
    }
};

template<class Key, class Value>
size_t
deep_sizeof(std::map<Key, Value> const& x)
{
    size_t size = sizeof(std::map<Key, Value>);
    for (auto const& i : x)
        size += deep_sizeof(i.first) + deep_sizeof(i.second);
    return size;
}

// OPTIONAL

template<class T>
struct type_info_query<optional<T>>
{
    static void
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_optional_type(get_type_info<T>());
    }
};

template<class T>
size_t
deep_sizeof(optional<T> const& x)
{
    using cradle::deep_sizeof;
    return sizeof(optional<T>) + (x ? deep_sizeof(*x) : 0);
}

template<class T>
void
to_dynamic(cradle::dynamic* v, optional<T> const& x)
{
    cradle::dynamic_map map;
    if (x)
    {
        to_dynamic(&map[cradle::dynamic("some")], *x);
    }
    else
    {
        to_dynamic(&map[cradle::dynamic("none")], cradle::nil);
    }
    *v = std::move(map);
}

CRADLE_DEFINE_EXCEPTION(invalid_optional_type)
CRADLE_DEFINE_ERROR_INFO(string, optional_type_tag)

template<class T>
void
from_dynamic(optional<T>* x, cradle::dynamic const& v)
{
    cradle::dynamic_map const& map = cradle::cast<cradle::dynamic_map>(v);
    string type;
    from_dynamic(&type, cradle::get_union_tag(map));
    if (type == "some")
    {
        try
        {
            T t;
            from_dynamic(&t, cradle::get_field(map, "some"));
            *x = t;
        }
        catch (boost::exception& e)
        {
            cradle::add_dynamic_path_element(e, "some");
            throw;
        }
    }
    else if (type == "none")
    {
        *x = none;
    }
    else
    {
        CRADLE_THROW(invalid_optional_type() << optional_type_tag_info(type));
    }
}

} // namespace cradle

namespace boost {

template<class T>
size_t
hash_value(optional<T> const& x)
{
    return x ? cradle::invoke_hash(x.get()) : 0;
}

} // namespace boost

#endif
