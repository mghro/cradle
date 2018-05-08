#include <cradle/encodings/yaml.hpp>

#include <cradle/core/testing.hpp>

using namespace cradle;

static string
strip_whitespace(string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

// Test that a YAML string can be translated to its expected dynamic form.
// (But don't test the inverse.)
static void
test_one_way_yaml_encoding(
    string const& yaml, dynamic const& expected_value, bool round_trip = true)
{
    CAPTURE(yaml)

    // Parse it and check that it matches.
    auto converted_value = parse_yaml_value(yaml);
    REQUIRE(converted_value == expected_value);
}

// Test that a YAML string can be translated to and from its expected dynamic
// form.
static void
test_yaml_encoding(
    string const& yaml, dynamic const& expected_value, bool round_trip = true)
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

// Test that dynamic value can be translated to the expected diagnostic
// encoding.
static void
test_diagnostic_yaml_encoding(dynamic const& value, string const& expected_yaml)
{
    auto yaml = value_to_diagnostic_yaml(value);
    REQUIRE(strip_whitespace(yaml) == strip_whitespace(expected_yaml));
}

TEST_CASE("basic YAML encoding", "[encodings][yaml]")
{
    // Try some basic types.
    test_yaml_encoding(
        R"(

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
    test_one_way_yaml_encoding(
        R"(
            0x10
        )",
        integer(16));
    test_one_way_yaml_encoding(
        R"(
            0o10
        )",
        integer(8));
    test_one_way_yaml_encoding(
        R"(
            "hi"
        )",
        "hi");

    // Try some arrays.
    test_yaml_encoding(
        R"(
            - 1
            - 2
            - 3
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
            happy: true
            n: 4.125
        )",
        {{"happy", true}, {"n", 4.125}});

    // Try a map with non-string keys.
    test_yaml_encoding(
        R"(
            false: 4.125
            0.1: xyz
        )",
        dynamic_map({{false, 4.125}, {0.1, "xyz"}}));

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
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:13:03.456ZABC"
        )",
        "2017-05-26T13:13:03.456ZABC");
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:XX:03.456Z"
        )",
        "2017-05-26T13:XX:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:03.456Z"
        )",
        "2017-05-26T13:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T42:00:03.456Z"
        )",
        "2017-05-26T42:00:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "X017-05-26T13:02:03.456Z"
        )",
        "X017-05-26T13:02:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "2X17-05-26T13:02:03.456Z"
        )",
        "2X17-05-26T13:02:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "20X7-05-26T13:02:03.456Z"
        )",
        "20X7-05-26T13:02:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "201X-05-26T13:02:03.456Z"
        )",
        "201X-05-26T13:02:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "2017X05-26T13:02:03.456Z"
        )",
        "2017X05-26T13:02:03.456Z");
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        "2017-05-26T13:02:03.456_");
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        "2017-05-26T13:02:03.456_");
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.45Z"
        )",
        "2017-05-26T13:02:03.45Z");

    // Try a blob.
    char blob_data[] = "some blob data";
    test_yaml_encoding(
        R"(
            type: base64-encoded-blob
            blob: c29tZSBibG9iIGRhdGE=
        )",
        blob(ownership_holder(), blob_data, sizeof(blob_data) - 1));

    // Try some other things that aren't blobs but look similar.
    test_yaml_encoding(
        R"(
            blob: 1
            type: blob
        )",
        {{"type", "blob"}, {"blob", integer(1)}});
    test_yaml_encoding(
        R"(
            blob: awe
            type: 12
        )",
        {{"type", integer(12)}, {"blob", "awe"}});
}

TEST_CASE("diagnostic YAML encoding", "[encodings][yaml]")
{
    char small_blob_data[] = "small blob";
    auto small_blob = blob(
        ownership_holder(), small_blob_data, sizeof(small_blob_data) - 1);
    test_diagnostic_yaml_encoding(
        small_blob,
        R"( |
            <blob>
            small blob
        )");

    auto large_blob = blob(ownership_holder(), 0, 16384);
    test_diagnostic_yaml_encoding(large_blob, "\"<blob - size: 16384 bytes>\"");

    char unprintable_blob_data[] = "\01blob";
    auto unprintable_blob = blob(
        ownership_holder(),
        unprintable_blob_data,
        sizeof(unprintable_blob_data) - 1);
    test_diagnostic_yaml_encoding(
        unprintable_blob, "\"<blob - size: 5 bytes>\"");

    test_diagnostic_yaml_encoding(
        dynamic_map({{false, small_blob}, {0.1, "xyz"}}),
        R"(
            false: |
              <blob>
              small blob
            0.1: xyz
        )");
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
