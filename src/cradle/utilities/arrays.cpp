#include <cradle/utilities/arrays.h>

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

}