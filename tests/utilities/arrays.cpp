#include <cradle/utilities/arrays.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("index bounds checking", "[core][utilities]")
{
    try
    {
        check_index_bounds("my_index", 12, 11);
        FAIL("no exception thrown");
    }
    catch (index_out_of_bounds& e)
    {
        REQUIRE(get_required_error_info<index_value_info>(e) == 12);
        REQUIRE(get_required_error_info<index_upper_bound_info>(e) == 11);
    }

    REQUIRE_NOTHROW(check_index_bounds("my_index", 11, 12));

    REQUIRE_THROWS_AS(
        check_index_bounds("my_index", 12, 12), index_out_of_bounds);
}

TEST_CASE("array size checking", "[core][utilities]")
{
    try
    {
        check_array_size(12, 11);
        FAIL("no exception thrown");
    }
    catch (array_size_mismatch& e)
    {
        REQUIRE(get_required_error_info<expected_size_info>(e) == 12);
        REQUIRE(get_required_error_info<actual_size_info>(e) == 11);
    }

    REQUIRE_NOTHROW(check_array_size(12, 12));
}
