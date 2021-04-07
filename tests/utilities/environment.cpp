#include <cradle/utilities/environment.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

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
