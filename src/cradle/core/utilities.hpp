#ifndef CRADLE_CORE_UTILITIES_HPP
#define CRADLE_CORE_UTILITIES_HPP

#include <cradle/core/exception.hpp>

#include <memory>
#include <type_traits>
#include <vector>

#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>

// TODO: Remove this?
#include <boost/optional/optional_io.hpp>

namespace cradle {

using boost::lexical_cast;

template<typename T>
struct array_deleter
{
    void
    operator()(T* p)
    {
        delete[] p;
    }
};

// CRADLE_LAMBDIFY(f) produces a lambda that calls f, which is essentially a
// version of f that can be passed as an argument and still allows normal
// overload resolution.
#define CRADLE_LAMBDIFY(f) [](auto&&... args) { return f(args...); }

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

// function_view is the non-owning equivalent of std::function.
template<class Signature>
class function_view;
template<class Return, class... Args>
class function_view<Return(Args...)>
{
 private:
    using signature_type = Return(void*, Args...);

    void* _ptr;
    Return (*_erased_fn)(void*, Args...);

 public:
    template<typename T>
    function_view(T&& x) noexcept : _ptr{(void*) std::addressof(x)}
    {
        _erased_fn = [](void* ptr, Args... xs) -> Return {
            return (*reinterpret_cast<std::add_pointer_t<T>>(ptr))(
                std::forward<Args>(xs)...);
        };
    }

    decltype(auto)
    operator()(Args... xs) const
        noexcept(noexcept(_erased_fn(_ptr, std::forward<Args>(xs)...)))
    {
        return _erased_fn(_ptr, std::forward<Args>(xs)...);
    }
};

} // namespace cradle

#endif
