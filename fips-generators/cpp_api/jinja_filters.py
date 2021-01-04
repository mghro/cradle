"""
This module provides all our custom filters for the Jinja2 environment.

Any globals defined in this file are automatically imported as filters in
Jinja2. Anything that shouldn't be imported should have a name that starts with
an underscore.
"""

import textwrap as _textwrap
from cpp_api.jinja_globals import union_tag as _union_tag


def format_comment(value, width=79):
    """Transform a string into a C++ comment with proper wrapping."""
    return "\n".join(
        map(lambda line: "// " + line, _textwrap.wrap(value, width - 3)))


def map_format(list_, pattern):
    """Invoke :pattern with each value in the input list (as '{0}' in the
       pattern) and return the resulting list of strings."""
    return map(pattern.format, list_)


def string_hash(s):
    """Hash a string."""
    return hex(hash(s) & 0x7fffffff)


def cpp_type_for_schema(schema, omissible=False):
    """Generate the C++ type for a schema."""

    def cpp_type_for_array(array):
        """Generate the C++ type for an array schema."""
        element_type = cpp_type_for_schema(array.element_schema)
        if hasattr(array, "size"):
            return "std::array<" + element_type + "," + str(int(
                array.size)) + ">"
        else:
            return "std::vector<" + element_type + ">"

    def cpp_type_for_map(map_info):
        """Generate the C++ type for a map schema."""
        return "std::map<" + cpp_type_for_schema(map_info.key_schema) + "," + \
            cpp_type_for_schema(map_info.value_schema) + ">"

    cases = {
        "nil": lambda _: "cradle::nil_t",
        "boolean": lambda _: "bool",
        "datetime": lambda _: "boost::posix_time::ptime",
        "integer": lambda _: "cradle::integer",
        "float": lambda _: "double",
        "string": lambda _: "std::string",
        "blob": lambda _: "cradle::blob",
        "optional":
        lambda t: "optional<" + cpp_type_for_schema(t) + ">",
        "array": cpp_type_for_array,
        "map": cpp_type_for_map,
        "reference": lambda _: "std::string",
        "named": lambda t: t.app + "::" + t.name,
        "dynamic": lambda t: "cradle::dynamic"
    }

    # Check the tag of the schema and invoke the appropriate case.
    tag = _union_tag(schema)
    cpp_type = cases[tag](getattr(schema, tag))

    # If the type is omissible, make the type optional.
    if omissible:
        cpp_type = "boost::optional<" + cpp_type + ">"

    return cpp_type
