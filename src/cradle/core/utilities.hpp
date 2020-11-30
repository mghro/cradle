#ifndef CRADLE_CORE_UTILITIES_HPP
#define CRADLE_CORE_UTILITIES_HPP

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <boost/any.hpp>
#include <boost/exception/all.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/stacktrace.hpp>

namespace cradle {

using std::string;

using boost::lexical_cast;

using boost::none;
using boost::optional;

typedef int64_t integer;

// some(x) creates a boost::optional of the proper type with the value of :x.
template<class T>
auto
some(T&& x)
{
    return optional<std::remove_reference_t<T>>(std::forward<T>(x));
}

using boost::any;

template<typename T>
struct array_deleter
{
    void
    operator()(T* p)
    {
        delete[] p;
    }
};

// ownership_holder is meant to express polymorphic ownership of a resource.
// The idea is that the resource may be owned in many different ways, and we
// don't care what way. We only want an object that will provide ownership of
// the resource until it's destructed. We can achieve this by using an any
// object to hold the ownership object.
typedef any ownership_holder;

// Invoke the standard hash function for a value.
template<class T>
size_t
invoke_hash(T const& x)
{
    return boost::hash<T>()(x);
}

// CRADLE_LAMBDIFY(f) produces a lambda that calls f, which is essentially a
// version of f that can be passed as an argument and still allows normal
// overload resolution.
#define CRADLE_LAMBDIFY(f) [](auto&&... args) { return f(args...); }

// The following macros are simple wrappers around Boost.Exception to codify
// how that library should be used within CRADLE.

#define CRADLE_DEFINE_EXCEPTION(id)                                           \
    struct id : virtual boost::exception, virtual std::exception              \
    {                                                                         \
        char const*                                                           \
        what() const noexcept                                                 \
        {                                                                     \
            return boost::diagnostic_information_what(*this);                 \
        }                                                                     \
    };

#define CRADLE_DEFINE_ERROR_INFO(T, id)                                       \
    typedef boost::error_info<struct id##_info_tag, T> id##_info;

CRADLE_DEFINE_ERROR_INFO(boost::stacktrace::stacktrace, stacktrace)

#define CRADLE_THROW(x)                                                       \
    BOOST_THROW_EXCEPTION(                                                    \
        (x) << stacktrace_info(boost::stacktrace::stacktrace()))

using boost::get_error_info;

// get_required_error_info is just like get_error_info except that it requires
// the info to be present and returns a const reference to it. If the info is
// missing, it throws its own exception.
CRADLE_DEFINE_EXCEPTION(missing_error_info)
CRADLE_DEFINE_ERROR_INFO(string, error_info_id)
CRADLE_DEFINE_ERROR_INFO(string, wrapped_exception_diagnostics)
template<class ErrorInfo, class Exception>
typename ErrorInfo::error_info::value_type const&
get_required_error_info(Exception const& e)
{
    typename ErrorInfo::error_info::value_type const* info
        = get_error_info<ErrorInfo>(e);
    if (!info)
    {
        CRADLE_THROW(
            missing_error_info()
            << error_info_id_info(typeid(ErrorInfo).name())
            << wrapped_exception_diagnostics_info(
                   boost::diagnostic_information(e)));
    }
    return *info;
}

// Check that an index is in bounds.
// :index must be nonnegative and strictly less than :upper_bound to pass.
void
check_index_bounds(string const& label, size_t index, size_t upper_bound);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(index_out_of_bounds)
CRADLE_DEFINE_ERROR_INFO(string, index_label)
CRADLE_DEFINE_ERROR_INFO(size_t, index_value)
CRADLE_DEFINE_ERROR_INFO(size_t, index_upper_bound)

// Check that an array size matches an expected size.
void
check_array_size(size_t expected_size, size_t actual_size);

// If the above check fails, it throws this exception.
CRADLE_DEFINE_EXCEPTION(array_size_mismatch)
CRADLE_DEFINE_ERROR_INFO(size_t, expected_size)
CRADLE_DEFINE_ERROR_INFO(size_t, actual_size)

// invalid_enum_value is thrown when an enum's raw (integer) value is invalid.
CRADLE_DEFINE_EXCEPTION(invalid_enum_value)
CRADLE_DEFINE_ERROR_INFO(string, enum_id)
CRADLE_DEFINE_ERROR_INFO(int, enum_value)

// invalid_enum_string is thrown when attempting to convert a string value to
// an enum and the string doesn't match any of the enum's cases.
CRADLE_DEFINE_EXCEPTION(invalid_enum_string)
// Note that this also uses the enum_id info declared above.
CRADLE_DEFINE_ERROR_INFO(string, enum_string)

// If a simple parsing operation fails, this exception can be thrown.
CRADLE_DEFINE_EXCEPTION(parsing_error)
CRADLE_DEFINE_ERROR_INFO(string, expected_format)
CRADLE_DEFINE_ERROR_INFO(string, parsed_text)
CRADLE_DEFINE_ERROR_INFO(string, parsing_error)

// Get the value of an environment variable.
string
get_environment_variable(string const& name);
// If the variable isn't set, the following exception is thrown.
CRADLE_DEFINE_EXCEPTION(missing_environment_variable)
CRADLE_DEFINE_ERROR_INFO(string, variable_name)

// Get the value of an optional environment variable.
// If the variable isn't set, this simply returns none.
optional<string>
get_optional_environment_variable(string const& name);

// Set the value of an environment variable.
void
set_environment_variable(string const& name, string const& value);

// If an error occurs internally within library that provides its own
// error messages, this is used to convey that message.
CRADLE_DEFINE_ERROR_INFO(string, internal_error_message)

// This can be used to flag errors that represent failed checks on conditions
// that should be guaranteed internally.
CRADLE_DEFINE_EXCEPTION(internal_check_failed)

// This exception is used when a low-level system call fails (one that should
// generally always work) and there is really no point in creating a specific
// exception type for it.
CRADLE_DEFINE_EXCEPTION(system_call_failed)
CRADLE_DEFINE_ERROR_INFO(string, failed_system_call)

// functional map over a vector
template<class Item, class Fn>
auto
map(Fn const& fn, std::vector<Item> const& items)
{
    typedef decltype(fn(Item())) mapped_item_type;
    size_t item_count = items.size();
    std::vector<mapped_item_type> result(item_count);
    for (size_t i = 0; i != item_count; ++i)
        result[i] = fn(items[i]);
    return result;
}

// functional map over a map
template<class Key, class Value, class Fn>
auto
map(Fn const& fn, std::map<Key, Value> const& items)
    -> std::map<Key, decltype(fn(Value()))>
{
    typedef decltype(fn(Value())) mapped_item_type;
    std::map<Key, mapped_item_type> result;
    for (auto const& item : items)
        result[item.first] = fn(item.second);
    return result;
}

// This captures information about the state of the Git repository for the
// source code itself.
struct repository_info
{
    // the abbreviated object name of the current commit
    string commit_object_name;

    // Does the repository have local modifications?
    bool dirty;

    // the closest tag in the history of the repository
    string tag;

    // how many commits there have been since the tag
    unsigned commits_since_tag;
};

// Does the given repository correspond to a tagged version of the code?
inline bool
is_tagged_version(repository_info const& info)
{
    return info.commits_since_tag == 0 && !info.dirty;
}

// A flag_set is a set of flags, each of which represents a boolean property.
// It is implemented as a simple unsigned integer, where each bit represents
// a different property.
// (The property codes must be defined manually as constants.)
// The advantage of using this over plain unsigned integers is that this is
// type-safe and slightly more explicit.
// A flag_set has a Tag type that identifies the set of properties that go with
// it. Only properties/sets with the same tag can be combined.

// NO_FLAGS can be implicitly converted to any type of flag_set.
struct null_flag_set
{
};
static null_flag_set const NO_FLAGS = null_flag_set();

template<class Tag>
struct flag_set
{
    unsigned code;
    flag_set()
    {
    }
    flag_set(null_flag_set) : code(0)
    {
    }
    explicit flag_set(unsigned code) : code(code)
    {
    }
    // allows use within if statements without other unintended conversions
    typedef unsigned flag_set::*unspecified_bool_type;
    operator unspecified_bool_type() const
    {
        return code != 0 ? &flag_set::code : 0;
    }
};

template<class Tag>
flag_set<Tag>
operator|(flag_set<Tag> a, flag_set<Tag> b)
{
    return flag_set<Tag>(a.code | b.code);
}
template<class Tag>
flag_set<Tag>&
operator|=(flag_set<Tag>& a, flag_set<Tag> b)
{
    a.code |= b.code;
    return a;
}
template<class Tag>
flag_set<Tag>
operator&(flag_set<Tag> a, flag_set<Tag> b)
{
    return flag_set<Tag>(a.code & b.code);
}
template<class Tag>
flag_set<Tag>&
operator&=(flag_set<Tag>& a, flag_set<Tag> b)
{
    a.code &= b.code;
    return a;
}
template<class Tag>
bool
operator==(flag_set<Tag> a, flag_set<Tag> b)
{
    return a.code == b.code;
}
template<class Tag>
bool
operator!=(flag_set<Tag> a, flag_set<Tag> b)
{
    return a.code != b.code;
}
template<class Tag>
bool
operator<(flag_set<Tag> a, flag_set<Tag> b)
{
    return a.code < b.code;
}
template<class Tag>
flag_set<Tag>
operator~(flag_set<Tag> a)
{
    return flag_set<Tag>(~a.code);
}

template<class Tag>
size_t
hash_value(cradle::flag_set<Tag> const& set)
{
    return set.code;
}

#define CRADLE_DEFINE_FLAG_TYPE(type_prefix)                                  \
    struct type_prefix##_flag_tag                                             \
    {                                                                         \
    };                                                                        \
    typedef cradle::flag_set<type_prefix##_flag_tag> type_prefix##_flag_set;

#define CRADLE_DEFINE_FLAG(type_prefix, code, name)                           \
    static unsigned const name##_CODE = code;                                 \
    static cradle::flag_set<type_prefix##_flag_tag> const name(code);

} // namespace cradle

#endif
