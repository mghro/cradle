#include <cradle/core/type_interfaces.h>

#include <cradle/utilities/testing.h>

#include <cradle/caching/disk_cache.hpp>
#include <cradle/utilities/text.h>

using namespace cradle;

TEST_CASE("nil type interface", "[core][types]")
{
    test_regular_value(nil);
}

TEST_CASE("nil hashing", "[core][types]")
{
    REQUIRE(invoke_hash(nil) == 0);
}

TEST_CASE("bool type interface", "[core][types]")
{
    test_regular_value_pair(false, true);
}

template<class Integer>
void
test_integer_interface()
{
    test_regular_value_pair(Integer(0), Integer(1));

    REQUIRE(deep_sizeof(Integer(0)) == sizeof(Integer));
}

TEST_CASE("integer type interfaces", "[core][types]")
{
    test_integer_interface<signed char>();
    test_integer_interface<unsigned char>();
    test_integer_interface<signed short>();
    test_integer_interface<unsigned short>();
    test_integer_interface<signed int>();
    test_integer_interface<unsigned int>();
    test_integer_interface<signed long>();
    test_integer_interface<unsigned long>();
    test_integer_interface<signed long long>();
    test_integer_interface<unsigned long long>();
}

template<class Float>
void
test_float_interface()
{
    test_regular_value_pair(Float(0.5), Float(1.5));

    REQUIRE(deep_sizeof(Float(0)) == sizeof(Float));
}

TEST_CASE("floating point type interfaces", "[core][types]")
{
    test_float_interface<float>();
    test_float_interface<double>();
}

TEST_CASE("string type interface", "[core][types]")
{
    test_regular_value_pair(string("hello"), string("world!"));

    REQUIRE(deep_sizeof(string("hello")) == deep_sizeof(string()) + 5);
}

TEST_CASE("date type interface", "[core][types]")
{
    test_regular_value_pair(
        boost::gregorian::date(2017, boost::gregorian::Apr, 26),
        boost::gregorian::date(2017, boost::gregorian::Apr, 27));

    // Try parsing a malformed date.
    try
    {
        from_dynamic<date>("asdf");
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "date");
        REQUIRE(get_required_error_info<parsed_text_info>(e) == "asdf");
    }
}

TEST_CASE("ptime type interface", "[core][types]")
{
    test_regular_value_pair(
        ptime(
            date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3)),
        ptime(
            boost::gregorian::date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 4)));
}

TEST_CASE("blob type interface", "[core][types]")
{
    char blob_data[] = {'a', 'b'};

    INFO("Test blobs of the different sizes.");
    test_regular_value_pair(
        blob{ownership_holder(), blob_data, 1},
        blob{ownership_holder(), blob_data, 2});

    INFO("Test blobs of the same size but with different data.");
    test_regular_value_pair(
        blob{ownership_holder(), blob_data, 1},
        blob{ownership_holder(), blob_data + 1, 1});
}

TEST_CASE("optional type interface", "[core][types]")
{
    // Test an optional with a value.
    test_regular_value_pair(some(string("hello")), some(string("world!")));

    REQUIRE(
        deep_sizeof(some(string("hello")))
        == sizeof(optional<string>) + deep_sizeof(string()) + 5);

    // Test an empty optional.
    test_regular_value(optional<string>());
    REQUIRE(deep_sizeof(optional<string>()) == sizeof(optional<string>));

    // Try converting an invalid dynamic value to an optional.
    try
    {
        from_dynamic<optional<string>>(dynamic({{"asdf", nil}}));
        FAIL("no exception thrown");
    }
    catch (invalid_optional_type& e)
    {
        REQUIRE(get_required_error_info<optional_type_tag_info>(e) == "asdf");
    }
}

TEST_CASE("vector type interface", "[core][types]")
{
    test_regular_value_pair(std::vector<int>({0, 1}), std::vector<int>({1}));

    REQUIRE(
        deep_sizeof(std::vector<int>({0, 1}))
        == deep_sizeof(std::vector<int>()) + deep_sizeof(0) + deep_sizeof(1));
}

TEST_CASE("map type interface", "[core][types]")
{
    test_regular_value(std::map<int, int>({}));

    test_regular_value_pair(
        std::map<int, int>({{0, 1}, {1, 2}}),
        std::map<int, int>({{1, 2}, {2, 5}, {3, 7}}));

    REQUIRE(
        deep_sizeof(std::map<int, int>({{0, 1}}))
        == deep_sizeof(std::map<int, int>()) + deep_sizeof(0)
               + deep_sizeof(1));
}

TEST_CASE("generated type interfaces", "[core][types]")
{
    {
        INFO("Test a generated structure type.");
        test_regular_value_pair(
            disk_cache_config(some(string("abc")), 12),
            disk_cache_config(some(string("def")), 1));
    }
}
