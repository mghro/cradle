#include <cradle/utilities/environment.h>

namespace cradle {

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
#ifdef _WIN32
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
