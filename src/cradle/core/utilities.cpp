#include <cradle/core/utilities.hpp>

#include <cstdlib>

namespace cradle {

void
check_index_bounds(string const& label, size_t index, size_t upper_bound)
{
    if (index >= upper_bound)
    {
        CRADLE_THROW(
            index_out_of_bounds()
            << index_label_info(label) << index_value_info(index)
            << index_upper_bound_info(upper_bound));
    }
}

void
check_array_size(size_t expected_size, size_t actual_size)
{
    if (expected_size != actual_size)
    {
        CRADLE_THROW(
            array_size_mismatch() << expected_size_info(expected_size)
                                  << actual_size_info(actual_size));
    }
}

string
get_environment_variable(string const& name)
{
    auto value = get_optional_environment_variable(name);
    if (!value)
    {
        CRADLE_THROW(
            missing_environment_variable() << variable_name_info(name));
    }
    return *value;
}

optional<string>
get_optional_environment_variable(string const& name)
{
    char const* value = std::getenv(name.c_str());
    return value && *value != '\0' ? some(string(value)) : none;
}

void
set_environment_variable(string const& name, string const& value)
{
#ifdef WIN32
    auto assignment = name + "=" + value;
    _putenv(assignment.c_str());
#else
    if (value.empty())
        unsetenv(name.c_str());
    else
        setenv(name.c_str(), value.c_str(), 1);
#endif
}

} // namespace cradle
