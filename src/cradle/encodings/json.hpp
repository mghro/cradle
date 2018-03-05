#ifndef CRADLE_ENCODINGS_JSON_HPP
#define CRADLE_ENCODINGS_JSON_HPP

#include <cradle/common.hpp>

// JSON - conversion to and from JSON strings

namespace cradle {

// Parse some JSON test into a dynamic value.
dynamic
parse_json_value(char const* json, size_t length);

// Same as above, but accepts a string.
static inline dynamic
parse_json_value(string const& json)
{
    return parse_json_value(json.c_str(), json.length());
}

// Write a value to a string in JSON format.
string
value_to_json(dynamic const& v);

// Write a value to a blob in JSON format.
// This does NOT include a terminating null character.
blob
value_to_json_blob(dynamic const& v);

} // namespace cradle

#endif
