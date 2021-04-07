#ifndef CRADLE_ENCODINGS_YAML_H
#define CRADLE_ENCODINGS_YAML_H

#include <cradle/core.h>

// YAML - conversion to and from YAML strings

namespace cradle {

// Parse some YAML text into a dynamic value.
dynamic
parse_yaml_value(char const* yaml, size_t length);

// Same as above, but accepts a string.
inline dynamic
parse_yaml_value(string const& yaml)
{
    return parse_yaml_value(yaml.c_str(), yaml.length());
}

// Write a value to a string in YAML format.
string
value_to_yaml(dynamic const& v);

// Write a value to a diagnostic string in YAML format.
// This won't necessarily capture the entire contents of the value. In
// particular, it will omit contents of large blobs.
string
value_to_diagnostic_yaml(dynamic const& v);

// Write a value to a blob in YAML format.
// This does NOT include a terminating null character.
blob
value_to_yaml_blob(dynamic const& v);

// Write a value to a blob in 'diagnostic' YAML format.
// This does NOT include a terminating null character.
blob
value_to_diagnostic_yaml_blob(dynamic const& v);

} // namespace cradle

#endif
