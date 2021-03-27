#ifndef CRADLE_DIFF_HPP
#define CRADLE_DIFF_HPP

#include <cradle/common.hpp>

namespace cradle {

api(enum internal)
enum class value_diff_op
{
    // insert a field into a record or an item into a list
    INSERT,
    // update an existing record field or list item
    UPDATE,
    // delete a record field or list item
    DELETE
};

// value_diff_path represents the path from the root of a value to the point
// where a change should be applied.
// Path elements can either be strings or positive integers.
// Strings represent record field names.
// Integers represent list indices.
typedef std::vector<dynamic> value_diff_path;

CRADLE_DEFINE_EXCEPTION(invalid_diff_path)

api(struct internal)
struct value_diff_item
{
    value_diff_path path;

    value_diff_op op;

    optional<dynamic> a, b;
};

typedef std::vector<value_diff_item> value_diff;

// Compute the difference between two dynamic values.
// Applying the resulting diff to a will yield b.
value_diff
compute_value_diff(dynamic const& a, dynamic const& b);

// Apply a diff to a value.
dynamic
apply_value_diff(dynamic const& v, value_diff const& diff);

} // namespace cradle

#endif
