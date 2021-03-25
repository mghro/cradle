#ifndef CRADLE_CORE_TYPE_DEFINITIONS_HPP
#define CRADLE_CORE_TYPE_DEFINITIONS_HPP

#include <any>
#include <map>
#include <string>

#include <boost/core/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <boost/optional.hpp>

#include <iostream>

namespace cradle {

using std::string;

using boost::none;
using boost::optional;

// some(x) creates a boost::optional of the proper type with the value of :x.
template<class T>
auto
some(T&& x)
{
    return optional<std::remove_reference_t<T>>(std::forward<T>(x));
}

typedef int64_t integer;

// ownership_holder is meant to express polymorphic ownership of a resource.
// The idea is that the resource may be owned in many different ways, and we
// don't care what way. We only want an object that will provide ownership of
// the resource until it's destructed. We can achieve this by using an any
// object to hold the ownership object.
typedef std::any ownership_holder;

// nil_t is a unit type. It has only one possible value, :nil.
struct nil_t
{
};
static nil_t nil;

struct blob
{
    ownership_holder ownership;
    char const* data = nullptr;
    std::size_t size = 0;
};

// type_info_query<T>::get(&info) should set :info to the CRADLE type info for
// the type T from the perspective of someone *using* T. This might not be the
// full definition of T (e.g., for named types, it's just the name).
//
// All CRADLE regular types must provide a specialization of this.
//
template<class T>
struct type_info_query
{
};

struct api_type_info;

// definitive_type_info_query<T>::get(&info) should set *info to the definitive
// CRADLE type info for the type T. This is always the full definition of the
// type, even for named types.
//
// The default implementation of this simply forwards to the regular query.
//
template<class T>
struct definitive_type_info_query : type_info_query<T>
{
};

// enum_type_info_query<T>::get(&info) should set :info to the enum type info
// for the type T.
//
// All CRADLE enum types must provide a specialization of this.
//
template<class T>
struct enum_type_info_query
{
};

struct api_enum_info;

// structure_field_type_info_adder<T>::add(&fields), where T is a structure
// type, adds the CRADLE type info for the fields of T to the map :fields.
//
// This should be implemented by all CRADLE structure types.
//
template<class T>
struct structure_field_type_info_adder
{
};

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
    std::any const&
    contents() const&
    {
        return value_;
    }

    // Get a non-const reference to the contents.
    // This should be used with caution.
    // cast<T>(dynamic) provides a safer interface to this.
    std::any&
    contents() &
    {
        return value_;
    }

    // Get an r-value reference to the contents.
    // This should be used with caution.
    // cast<T>(dynamic) provides a safer interface to this.
    std::any&&
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
    std::any value_;
};

// omissible<T> is the same as optional<T>, but it obeys thinknode's behavior
// foe omissible fields. (It should only be used as a field in a structure.)
// optional<T> stores an optional value of type T (or no value).
template<class T>
struct omissible
{
    typedef T value_type;
    omissible() : valid_(false)
    {
    }
    omissible(T const& value) : value_(value), valid_(true)
    {
    }
    omissible(boost::none_t) : valid_(false)
    {
    }
    omissible(optional<T> const& opt)
        : value_(opt ? opt.get() : T()), valid_(opt ? true : false)
    {
    }
    omissible&
    operator=(T const& value)
    {
        value_ = value;
        valid_ = true;
        return *this;
    }
    omissible& operator=(boost::none_t)
    {
        valid_ = false;
        return *this;
    }
    omissible&
    operator=(optional<T> const& opt)
    {
        valid_ = opt ? true : false;
        value_ = opt ? opt.get() : T();
        return *this;
    }
    // allows use within if statements without other unintended conversions
    typedef bool omissible::*unspecified_bool_type;
    operator unspecified_bool_type() const
    {
        return valid_ ? &omissible::valid_ : 0;
    }
    operator optional<T>() const
    {
        return valid_ ? optional<T>(value_) : optional<T>();
    }
    T const&
    get() const
    {
        assert(valid_);
        return value_;
    }
    T&
    get()
    {
        assert(valid_);
        return value_;
    }
    T const&
    operator*() const
    {
        assert(valid_);
        return value_;
    }
    T&
    operator*()
    {
        assert(valid_);
        return value_;
    }
    T const*
    operator->() const
    {
        assert(valid_);
        return &value_;
    }
    T*
    operator->()
    {
        assert(valid_);
        return &value_;
    }

 private:
    T value_;
    bool valid_;
};

// IMMUTABLES

struct untyped_immutable_value : boost::noncopyable
{
    virtual ~untyped_immutable_value()
    {
    }
    virtual api_type_info
    type_info() const = 0;
    virtual size_t
    deep_size() const = 0;
    virtual size_t
    hash() const = 0;
    virtual dynamic
    as_dynamic() const = 0;
    virtual bool
    equals(untyped_immutable_value const* other) const = 0;
};

struct untyped_immutable
{
    std::shared_ptr<untyped_immutable_value> ptr;
};

} // namespace cradle

#endif
