#include <cradle/encodings/yaml.hpp>

#include <cradle/core/testing.hpp>

using namespace cradle;

static string
strip_whitespace(string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

// Test that a YAML string can be translated to and from its expected dynamic
// form.
static void
test_yaml_encoding(string const& yaml, dynamic const& expected_value)
{
    CAPTURE(yaml)

    // Parse it and check that it matches.
    auto converted_value = parse_yaml_value(yaml);
    REQUIRE(converted_value == expected_value);

    // Convert it back to YAML and check that that matches the original (modulo
    // whitespace).
    auto converted_yaml = value_to_yaml(converted_value);
    REQUIRE(strip_whitespace(converted_yaml) == strip_whitespace(yaml));

    // Also try it as a blob.
    auto yaml_blob = value_to_yaml_blob(converted_value);
    REQUIRE(
        string(
            reinterpret_cast<char const*>(yaml_blob.data),
            reinterpret_cast<char const*>(yaml_blob.data) + yaml_blob.size)
        == converted_yaml);
}

TEST_CASE("basic YAML encoding", "[encodings][yaml]")
{
    // Try some basic types.
    test_yaml_encoding(
        R"(
            null
        )",
        nil);
    test_yaml_encoding(
        R"(
            false
        )",
        false);
    test_yaml_encoding(
        R"(
            true
        )",
        true);
    test_yaml_encoding(
        R"(
            "true"
        )",
        "true");
    test_yaml_encoding(
        R"(
            1
        )",
        integer(1));
    test_yaml_encoding(
        R"(
            -1
        )",
        integer(-1));
    test_yaml_encoding(
        R"(
            1.25
        )",
        1.25);
    test_yaml_encoding(
        R"(
            "1.25"
        )",
        "1.25");
    test_yaml_encoding(
        R"(
            0x10
        )",
        integer(16));
    test_yaml_encoding(
        R"(
            0o10
        )",
        integer(8));
    test_yaml_encoding(
        R"(
            "hi"
        )",
        "hi");

    // Try some arrays.
    test_yaml_encoding(
        R"(
            [ 1, 2, 3 ]
        )",
        dynamic({integer(1), integer(2), integer(3)}));
    test_yaml_encoding(
        R"(
            []
        )",
        dynamic_array());

    // Try a map with string keys.
    test_yaml_encoding(
        R"(
            {
                happy: true,
                n: 4.125
            }
        )",
        {{"happy", true}, {"n", 4.125}});

    // Try a map with non-string keys.
    test_yaml_encoding(
        R"(
            {
                0.1: xyz,
                false: 4.125
            }
        )",
        dynamic_map({{0.1, "xyz"}, {false, 4.125}}));

    // Try some ptimes.
    test_yaml_encoding(
        R"(
            "2017-04-26T01:02:03.000Z"
        )",
        ptime(
            date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3)));
    test_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456Z"
        )",
        ptime(
            date(2017, boost::gregorian::May, 26),
            boost::posix_time::time_duration(13, 2, 3)
                + boost::posix_time::milliseconds(456)));

    // Try some thing that look like a ptime at first and check that they're
    // just treated as strings.
    test_yaml_encoding(
        R"(
            "2017-05-26T13:13:03.456ZABC"
        )",
        "2017-05-26T13:13:03.456ZABC");
    test_yaml_encoding(
        R"(
            "2017-05-26T13:XX:03.456Z"
        )",
        "2017-05-26T13:XX:03.456Z");
    test_yaml_encoding(
        R"(
            "2017-05-26T13:03.456Z"
        )",
        "2017-05-26T13:03.456Z");
    test_yaml_encoding(
        R"(
            "2017-05-26T42:00:03.456Z"
        )",
        "2017-05-26T42:00:03.456Z");
    test_yaml_encoding(
        R"(
            "X017-05-26T13:02:03.456Z"
        )",
        "X017-05-26T13:02:03.456Z");
    test_yaml_encoding(
        R"(
            "2X17-05-26T13:02:03.456Z"
        )",
        "2X17-05-26T13:02:03.456Z");
    test_yaml_encoding(
        R"(
            "20X7-05-26T13:02:03.456Z"
        )",
        "20X7-05-26T13:02:03.456Z");
    test_yaml_encoding(
        R"(
            "201X-05-26T13:02:03.456Z"
        )",
        "201X-05-26T13:02:03.456Z");
    test_yaml_encoding(
        R"(
            "2017X05-26T13:02:03.456Z"
        )",
        "2017X05-26T13:02:03.456Z");
    test_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        "2017-05-26T13:02:03.456_");
    test_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        "2017-05-26T13:02:03.456_");
    test_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.45Z"
        )",
        "2017-05-26T13:02:03.45Z");

    // Try a blob.
    char blob_data[] = "some blob data";
    test_yaml_encoding(
        R"(
            {
                blob: c29tZSBibG9iIGRhdGE=,
                type: base64-encoded-blob
            }
        )",
        blob(ownership_holder(), blob_data, sizeof(blob_data) - 1));

    // Try some other things that aren't blobs but look similar.
    test_yaml_encoding(
        R"(
            {
                blob: null,
                type: blob
            }
        )",
        {{"type", "blob"}, {"blob", nil}});
    test_yaml_encoding(
        R"(
            {
                blob: "awe",
                type: 12
            }
        )",
        {{"type", integer(12)}, {"blob", "awe"}});
}

TEST_CASE("malformed YAML blob", "[encodings][yaml]")
{
    try
    {
        parse_yaml_value(
            R"(
                {
                    type: base64-encoded-blob
                }
            )");
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(
            get_required_error_info<expected_format_info>(e)
            == "base64-encoded-blob");
        REQUIRE(
            strip_whitespace(get_required_error_info<parsed_text_info>(e))
            == strip_whitespace(
                   R"(
                    {
                        type: base64-encoded-blob
                    }
                )"));
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }

    try
    {
        parse_yaml_value(
            R"(
                {
                    foo: 12,
                    bar: {
                        blob: 4,
                        type: base64-encoded-blob
                    }
                }
            )");
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "base64");
        REQUIRE(get_required_error_info<parsed_text_info>(e) == "4");
    }
}

static void
test_malformed_yaml(string const& malformed_yaml)
{
    CAPTURE(malformed_yaml);

    try
    {
        parse_yaml_value(malformed_yaml);
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "YAML");
        REQUIRE(get_required_error_info<parsed_text_info>(e) == malformed_yaml);
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }
}

TEST_CASE("malformed YAML", "[encodings][yaml]")
{
    test_malformed_yaml(
        R"(
            ]asdf
        )");
    test_malformed_yaml(
        R"(
            asdf: [123
        )");
}