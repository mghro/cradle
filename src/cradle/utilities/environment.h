#ifndef CRADLE_UTILITIES_ENVIRONMENT_H
#define CRADLE_UTILITIES_ENVIRONMENT_H

#include <cradle/core.h>

namespace cradle {

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

} // namespace cradle

#endif
