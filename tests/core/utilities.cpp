#include <cradle/core/utilities.hpp>

#include <cradle/core/testing.hpp>

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

TEST_CASE("error info", "[core][utilities]")
{
    parsing_error error;
    error << parsed_text_info("asdf");

    REQUIRE(get_required_error_info<parsed_text_info>(error) == "asdf");

    try
    {
        get_required_error_info<expected_format_info>(error);
        FAIL("no exception thrown");
    }
    catch (missing_error_info& e)
    {
        get_required_error_info<error_info_id_info>(e);
        get_required_error_info<wrapped_exception_diagnostics_info>(e);
    }
}

TEST_CASE("environment variables", "[core][utilities]")
{
    string var_name = "some_unlikely_env_variable_awapwogj";

    REQUIRE(get_optional_environment_variable(var_name) == none);

    try
    {
        get_environment_variable(var_name);
        FAIL("no exception thrown");
    }
    catch (missing_environment_variable& e)
    {
        REQUIRE(get_required_error_info<variable_name_info>(e) == var_name);
    }

    string new_value = "nv";

    set_environment_variable(var_name, new_value);

    REQUIRE(get_environment_variable(var_name) == new_value);
    REQUIRE(get_optional_environment_variable(var_name) == some(new_value));
}
