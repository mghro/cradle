#ifndef CRADLE_CORE_TYPE_INTERFACES_HPP
#define CRADLE_CORE_TYPE_INTERFACES_HPP

#include <map>
#include <vector>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cradle/core/dynamic.hpp>

// This file provides implementations of the CRADLE Regular interface
// for all the core CRADLE types.

namespace cradle {

// NIL

bool static inline
operator==(nil_t a, nil_t b)
{ return true; }

bool static inline
operator!=(nil_t a, nil_t b)
{ return false; }

bool static inline
operator<(nil_t a, nil_t b)
{ return false; }

template<>
struct type_info_query<nil_t>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_nil(api_nil_type());
    }
};

size_t static inline
deep_sizeof(nil_t)
{
    return 0;
}

struct dynamic;

// Note that we don't have to do anything here because callers of to_dynamic are required to
// provide an uninitialized dynamic, which defaults to nil.
void static inline
to_dynamic(dynamic* v, nil_t n)
{}

void static inline
from_dynamic(nil_t* n, dynamic const& v)
{}

size_t static inline
hash_value(nil_t x)
{
    return 0;
}

// BOOL

template<>
struct type_info_query<bool>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_boolean(api_boolean_type());
    }
};

size_t static inline
deep_sizeof(bool)
{
    return sizeof(bool);
}

void
to_dynamic(dynamic* v, bool x);

void
from_dynamic(bool* x, dynamic const& v);

// INTEGERS AND FLOATS

#define CRADLE_DECLARE_NUMBER_INTERFACE(T) \
    void \
    to_dynamic(dynamic* v, T x); \
    \
    void \
    from_dynamic(T* x, dynamic const& v); \
    \
    size_t static inline \
    deep_sizeof(T) { return sizeof(T); }

#define CRADLE_DECLARE_INTEGER_INTERFACE(T) \
    template<> \
    struct type_info_query<T> \
    { \
        void static \
        get(api_type_info* info) \
        { \
            *info = make_api_type_info_with_integer(api_integer_type()); \
        } \
    }; \
    \
    integer \
    to_integer(T x); \
    \
    void \
    from_integer(T* x, integer n); \
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

#define CRADLE_DECLARE_FLOAT_INTERFACE(T) \
    template<> \
    struct type_info_query<T> \
    { \
        void static \
        get(api_type_info* info) \
        { \
            *info = make_api_type_info_with_float(api_float_type()); \
        } \
    }; \
    CRADLE_DECLARE_NUMBER_INTERFACE(T)

CRADLE_DECLARE_FLOAT_INTERFACE(float)
CRADLE_DECLARE_FLOAT_INTERFACE(double)

// STRING

template<>
struct type_info_query<string>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_string(api_string_type());
    }
};

size_t static inline
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
string to_string(date const& d);

void
to_dynamic(dynamic* v, date const& x);

void
from_dynamic(date* x, dynamic const& v);

template<>
struct type_info_query<date>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_string(api_string_type());
    }
};

size_t static inline
deep_sizeof(date)
{
    return sizeof(date);
}

}

namespace boost { namespace gregorian {

size_t static inline
hash_value(date const& x)
{
    return cradle::invoke_hash(cradle::to_string(x));
}

}}

namespace cradle {

// PTIME

using boost::posix_time::ptime;

// Get the preferred user-readable string representation of a ptime.
string to_string(ptime const& t);

// Get the preferred representation for encoding a ptime as a string.
// (This preserves milliseconds.)
string
to_value_string(ptime const& t);

void
to_dynamic(dynamic* v, ptime const& x);

void
from_dynamic(ptime* x, dynamic const& v);

template<>
struct type_info_query<ptime>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_datetime(api_datetime_type());
    }
};

size_t static inline
deep_sizeof(ptime)
{
    return sizeof(ptime);
}

}

namespace boost { namespace posix_time {

size_t static inline
hash_value(ptime const& x)
{
    return cradle::invoke_hash(cradle::to_string(x));
}

}}

namespace cradle {

// BLOB

bool
operator==(blob const& a, blob const& b);

bool static inline
operator!=(blob const& a, blob const& b)
{ return !(a == b); }

bool
operator<(blob const& a, blob const& b);

template<>
struct type_info_query<blob>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_blob(api_blob_type());
    }
};

size_t static inline
deep_sizeof(blob const& b)
{
    // This ignores the size of the ownership holder, but that's not a big deal.
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
make_string_blob(string const& s);
blob
make_string_blob(string&& s);

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
    void static
    get(api_type_info* info)
    {
        api_array_info array_info;
        array_info.element_schema = get_type_info<T>();
        array_info.size = none;
        *info = make_api_type_info_with_array(array_info);
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
to_dynamic(dynamic* v, std::array<T,N> const& x)
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
from_dynamic(std::array<T,N>* x, dynamic const& v)
{
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
struct type_info_query<std::array<T,N>>
{
    void static
    get(api_type_info* info)
    {
        api_array_info array_info;
        array_info.element_schema = get_type_info<T>();
        array_info.size = some(N);
        *info = make_api_type_info_with_array(array_info);
    }
};

template<class T, size_t N>
size_t
deep_sizeof(std::array<T,N> const& x)
{
    size_t size = 0;
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

// STD::MAP

template<class Key, class Value>
void
to_dynamic(dynamic* v, std::map<Key,Value> const& x)
{
    dynamic_map record;
    for (auto const& i : x)
        to_dynamic(&record[to_dynamic(i.first)], i.second);
    *v = std::move(record);
}

template<class Key, class Value>
void
from_dynamic(std::map<Key,Value>* x, dynamic const& v)
{
    x->clear();
    dynamic_map const& record = cast<dynamic_map>(v);
    for (auto const& i : record)
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
struct type_info_query<std::map<Key,Value>>
{
    void static
    get(api_type_info* info)
    {
        api_map_info map_info;
        map_info.key_schema = get_type_info<Key>();
        map_info.value_schema = get_type_info<Value>();
        *info = make_api_type_info_with_map(map_info);
    }
};

template<class Key, class Value>
size_t
deep_sizeof(std::map<Key,Value> const& x)
{
    size_t size = sizeof(std::map<Key,Value>);
    for (auto const& i : x)
        size += deep_sizeof(i.first) + deep_sizeof(i.second);
    return size;
}

// OPTIONAL

template<class T>
struct type_info_query<optional<T>>
{
    void static
    get(api_type_info* info)
    {
        *info = make_api_type_info_with_optional(get_type_info<T>());
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
    cradle::dynamic_map record;
    if (x)
    {
        to_dynamic(&record[cradle::dynamic("some")], *x);
    }
    else
    {
        to_dynamic(&record[cradle::dynamic("none")], cradle::nil);
    }
    *v = std::move(record);
}

CRADLE_DEFINE_EXCEPTION(invalid_optional_type)
CRADLE_DEFINE_ERROR_INFO(string, optional_type_tag)

template<class T>
void
from_dynamic(optional<T>* x, cradle::dynamic const& v)
{
    cradle::dynamic_map const& record = cradle::cast<cradle::dynamic_map>(v);
    string type;
    from_dynamic(&type, cradle::get_union_value_type(record));
    if (type == "some")
    {
        T t;
        from_dynamic(&t, cradle::get_field(record, "some"));
        *x = t;
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

}

namespace boost {

template<class T>
size_t
hash_value(optional<T> const& x)
{
    return x ? cradle::invoke_hash(x.get()) : 0;
}

}

#endif
