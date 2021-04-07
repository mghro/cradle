#include <cradle/encodings/json.hpp>

#include <cradle/core/testing.h>

using namespace cradle;

static string
strip_whitespace(string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

// Test that a JSON string can be translated to and from its expected dynamic
// form.
static void
test_json_encoding(string const& json, dynamic const& expected_value)
{
    CAPTURE(json);

    // Parse it and check that it matches.
    auto converted_value = parse_json_value(json);
    REQUIRE(converted_value == expected_value);

    // Convert it back to JSON and check that that matches the original (modulo
    // whitespace).
    auto converted_json = value_to_json(converted_value);
    REQUIRE(strip_whitespace(converted_json) == strip_whitespace(json));

    // Also try it as a blob.
    auto json_blob = value_to_json_blob(converted_value);
    REQUIRE(
        string(
            reinterpret_cast<char const*>(json_blob.data),
            reinterpret_cast<char const*>(json_blob.data) + json_blob.size)
        == converted_json);
}

TEST_CASE("basic JSON encoding", "[encodings][json]")
{
    // Try some basic types.
    test_json_encoding(
        R"(
            null
        )",
        nil);
    test_json_encoding(
        R"(
            false
        )",
        false);
    test_json_encoding(
        R"(
            true
        )",
        true);
    test_json_encoding(
        R"(
            1
        )",
        integer(1));
    test_json_encoding(
        R"(
            10737418240
        )",
        integer(10737418240));
    test_json_encoding(
        R"(
            -1
        )",
        integer(-1));
    test_json_encoding(
        R"(
            1.25
        )",
        1.25);
    test_json_encoding(
        R"(
            "hi"
        )",
        "hi");

    // Try some arrays.
    test_json_encoding(
        R"(
            [ 1, 2, 3 ]
        )",
        dynamic({integer(1), integer(2), integer(3)}));
    test_json_encoding(
        R"(
            []
        )",
        dynamic_array());

    // Try a map with string keys.
    test_json_encoding(
        R"(
            {
                "happy": true,
                "n": 4.125
            }
        )",
        {{"happy", true}, {"n", 4.125}});

    // Try a map with non-string keys.
    test_json_encoding(
        R"(
            [
                {
                    "key": false,
                    "value": "no"
                },
                {
                    "key": true,
                    "value": "yes"
                }
            ]
        )",
        dynamic_map({{false, "no"}, {true, "yes"}}));

    // Try some other JSON that looks like the above.
    test_json_encoding(
        R"(
            [
                {
                    "key": false
                },
                {
                    "key": true
                }
            ]
        )",
        {{{"key", false}}, {{"key", true}}});
    test_json_encoding(
        R"(
            [
                {
                    "key": false,
                    "valu": "no"
                },
                {
                    "key": true,
                    "valu": "yes"
                }
            ]
        )",
        {{{"key", false}, {"valu", "no"}}, {{"key", true}, {"valu", "yes"}}});
    test_json_encoding(
        R"(
            [
                {
                    "ke": false,
                    "value": "no"
                },
                {
                    "ke": true,
                    "value": "yes"
                }
            ]
        )",
        {{{"ke", false}, {"value", "no"}}, {{"ke", true}, {"value", "yes"}}});

    // Try some ptimes.
    test_json_encoding(
        R"(
            "2017-04-26T01:02:03.000Z"
        )",
        ptime(
            date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3)));
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.456Z"
        )",
        ptime(
            date(2017, boost::gregorian::May, 26),
            boost::posix_time::time_duration(13, 2, 3)
                + boost::posix_time::milliseconds(456)));

    // Try some thing that look like a ptime at first and check that they're
    // just treated as strings.
    test_json_encoding(
        R"(
            "2017-05-26T13:13:03.456ZABC"
        )",
        "2017-05-26T13:13:03.456ZABC");
    test_json_encoding(
        R"(
            "2017-05-26T13:XX:03.456Z"
        )",
        "2017-05-26T13:XX:03.456Z");
    test_json_encoding(
        R"(
            "2017-05-26T13:03.456Z"
        )",
        "2017-05-26T13:03.456Z");
    test_json_encoding(
        R"(
            "2017-05-26T42:00:03.456Z"
        )",
        "2017-05-26T42:00:03.456Z");
    test_json_encoding(
        R"(
            "X017-05-26T13:02:03.456Z"
        )",
        "X017-05-26T13:02:03.456Z");
    test_json_encoding(
        R"(
            "2X17-05-26T13:02:03.456Z"
        )",
        "2X17-05-26T13:02:03.456Z");
    test_json_encoding(
        R"(
            "20X7-05-26T13:02:03.456Z"
        )",
        "20X7-05-26T13:02:03.456Z");
    test_json_encoding(
        R"(
            "201X-05-26T13:02:03.456Z"
        )",
        "201X-05-26T13:02:03.456Z");
    test_json_encoding(
        R"(
            "2017X05-26T13:02:03.456Z"
        )",
        "2017X05-26T13:02:03.456Z");
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        "2017-05-26T13:02:03.456_");
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        "2017-05-26T13:02:03.456_");
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.45Z"
        )",
        "2017-05-26T13:02:03.45Z");

    // Try a blob.
    char blob_data[] = "some blob data";
    test_json_encoding(
        R"(
            {
                "blob": "c29tZSBibG9iIGRhdGE=",
                "type": "base64-encoded-blob"
            }
        )",
        blob{ownership_holder(), blob_data, sizeof(blob_data) - 1});

    // Try some other things that aren't blobs but look similar.
    test_json_encoding(
        R"(
            {
                "blob": "asdf",
                "type": "blob"
            }
        )",
        {{"type", "blob"}, {"blob", "asdf"}});
    test_json_encoding(
        R"(
            {
                "blob": "asdf",
                "type": 12
            }
        )",
        {{"type", integer(12)}, {"blob", "asdf"}});
}

TEST_CASE("malformed JSON blob", "[encodings][json]")
{
    try
    {
        parse_json_value(
            R"(
                {
                    "type": "base64-encoded-blob"
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
                        "type": "base64-encoded-blob"
                    }
                )"));
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }

    try
    {
        parse_json_value(
            R"(
                {
                    "foo": 12,
                    "bar": {
                        "blob": 4,
                        "type": "base64-encoded-blob"
                    }
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
                        "blob": 4,
                        "type": "base64-encoded-blob"
                    }
                )"));
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }
}

static void
test_malformed_json(string const& malformed_json)
{
    CAPTURE(malformed_json);

    try
    {
        parse_json_value(malformed_json);
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "JSON");
        REQUIRE(
            get_required_error_info<parsed_text_info>(e) == malformed_json);
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }
}

TEST_CASE("malformed JSON", "[encodings][json]")
{
    test_malformed_json(
        R"(
            asdf
        )");
    test_malformed_json(
        R"(
            asdf: 123
        )");
}
