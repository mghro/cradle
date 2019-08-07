#ifndef CRADLE_DIFF_HPP
#define CRADLE_DIFF_HPP

#include <cradle/core/diff_types.hpp>

namespace cradle {

CRADLE_DEFINE_EXCEPTION(invalid_diff_path)

// Compute the difference between two dynamic values.
// Applying the resulting diff to a will yield b.
value_diff
compute_value_diff(dynamic const& a, dynamic const& b);

// Apply a diff to a value.
dynamic
apply_value_diff(dynamic const& v, value_diff const& diff);

} // namespace cradle

#endif
