#ifndef CRADLE_CORE_TYPE_DEFINITIONS_HPP
#define CRADLE_CORE_TYPE_DEFINITIONS_HPP

#include <cradle/core/utilities.hpp>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <iostream>

#ifdef CRADLE_USING_TAGGED_CONSTRUCTORS
#include <boost/hana.hpp>
#endif

namespace cradle {

// The following utilities are used by the generated tagged constructors.
#ifdef CRADLE_USING_TAGGED_CONSTRUCTORS
template<class Arg>
struct is_hana_pair
{
    static bool const value = false;
};
template<class First, class Second>
struct is_hana_pair<boost::hana::pair<First, Second>>
{
    static bool const value = true;
};
template<class... Args>
struct has_hana_pair
{
    static bool const value = false;
};
template<class Arg, class... Rest>
struct has_hana_pair<Arg, Rest...>
{
    static bool const value
        = is_hana_pair<Arg>::value || has_hana_pair<Rest...>::value;
};
#endif

// nil_t is a unit type. It has only one possible value, :nil.
struct nil_t
{
};
static nil_t nil;

struct blob
{
    ownership_holder ownership;
    void const* data;
    std::size_t size;

    blob() : data(0), size(0)
    {
    }

    blob(ownership_holder const& ownership, void const* data, std::size_t size)
        : ownership(ownership), data(data), size(size)
    {
    }
};

// type_info_query<T>::get(&info) should fill info with the CRADLE type info for
// the type T. All CRADLE regular types must provide a specialization of this.
template<class T>
struct type_info_query
{
};

struct api_type_info;

struct dynamic;

enum class value_type
{
    NIL, // nil_t - no value
    BOOLEAN, // bool
    INTEGER, // integer
    FLOAT, // double
    STRING, // string
    BLOB, // blob - binary blob
    DATETIME, // boost::posix_time::ptime
    ARRAY, // dynamic_array - array of dynamic values
    MAP, // dynamic_map - collection of named dynamic values
};

// Arrays are represented as std::vectors and can be manipulated as such.
typedef std::vector<dynamic> dynamic_array;

// Maps are represented as std::maps and can be manipulated as such.
typedef std::map<dynamic, dynamic> dynamic_map;

struct dynamic
{
    // CONSTRUCTORS

    // Default construction creates a nil value.
    dynamic()
    {
        set(nil);
    }

    // Construct a dynamic from one of the base types.
    dynamic(nil_t v)
    {
        set(v);
    }
    dynamic(bool v)
    {
        set(v);
    }
    dynamic(integer v)
    {
        set(v);
    }
    dynamic(double v)
    {
        set(v);
    }
    dynamic(string const& v)
    {
        set(v);
    }
    dynamic(string&& v)
    {
        set(std::move(v));
    }
    dynamic(char const* v)
    {
        set(string(v));
    }
    dynamic(blob const& v)
    {
        set(v);
    }
    dynamic(blob&& v)
    {
        set(std::move(v));
    }
    dynamic(boost::posix_time::ptime const& v)
    {
        set(v);
    }
    dynamic(boost::posix_time::ptime&& v)
    {
        set(std::move(v));
    }
    dynamic(dynamic_array const& v)
    {
        set(v);
    }
    dynamic(dynamic_array&& v)
    {
        set(std::move(v));
    }
    dynamic(dynamic_map const& v)
    {
        set(v);
    }
    dynamic(dynamic_map&& v)
    {
        set(std::move(v));
    }

    // Construct from an initializer list.
    dynamic(std::initializer_list<dynamic> list);

    // GETTERS

    // Get the type of value stored here.
    value_type
    type() const
    {
        return type_;
    }

    // Get the contents.
    // This should be used with caution.
    // cast<T>(dynamic) provides a safer interface to this.
    any const&
    contents() const&
    {
        return value_;
    }

    // Get an r-value reference to the contents.
    // This should be used with caution.
    // cast<T>(dynamic) provides a safer interface to this.
    any&&
    contents() &&
    {
        return std::move(value_);
    }

 private:
    void
    set(nil_t _);
    void
    set(bool v);
    void
    set(integer v);
    void
    set(double v);
    void
    set(string const& v);
    void
    set(string&& v);
    void
    set(char const* v)
    {
        set(string(v));
    }
    void
    set(blob const& v);
    void
    set(blob&& v);
    void
    set(boost::posix_time::ptime const& v);
    void
    set(boost::posix_time::ptime&& v);
    void
    set(dynamic_array const& v);
    void
    set(dynamic_array&& v);
    void
    set(dynamic_map const& v);
    void
    set(dynamic_map&& v);

    friend void
    swap(dynamic& a, dynamic& b);

    value_type type_;
    any value_;
};

} // namespace cradle

#endif
