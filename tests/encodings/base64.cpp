#include <cradle/encodings/base64.h>

#include <boost/scoped_array.hpp>

#include <cradle/utilities/testing.h>
#include <cradle/utilities/text.h>

using namespace cradle;

// This tests the entire base64 encode/decode interface on a single string.
static void
test_base64_encoding(
    string const& original,
    string const& correct_encoding,
    base64_character_set const& character_set)
{
    INFO("Testing base64 encoding and decoding.");

    CAPTURE(original);
    CAPTURE(correct_encoding);

    // Check the encoded form.
    auto encoded = base64_encode(original, character_set);
    REQUIRE(encoded == correct_encoding);

    // Check that the encoded length calculation is within spec.
    {
        auto calculated = get_base64_encoded_length(original.length());
        auto actual = encoded.length() + 1;
        REQUIRE(calculated - 2 <= actual);
        REQUIRE(actual <= calculated);
    }

    // Check that decoding the encoded form gives the original string.
    auto decoded = base64_decode(encoded, character_set);
    REQUIRE(decoded == original);

    // Check that the decoded length calculation is within spec.
    {
        auto calculated = get_base64_decoded_length(encoded.length());
        auto actual = decoded.length();
        REQUIRE(calculated - 2 <= actual);
        REQUIRE(actual <= calculated);
    }
}

TEST_CASE("MIME base64 encoding", "[encodings][base64]")
{
    test_base64_encoding(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
        "TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2NpbmcgZ"
        "W"
        "xpdC4=",
        get_mime_base64_character_set());

    test_base64_encoding(
        "Proin sollicitudin cursus bibendum. Aliquam tempus eu mauris in "
        "varius. "
        "Quisque vulputate porttitor nisl, non scelerisque erat eleifend sed.",
        "UHJvaW4gc29sbGljaXR1ZGluIGN1cnN1cyBiaWJlbmR1bS4gQWxpcXVhbSB0ZW1wdXMgZ"
        "X"
        "UgbWF1cmlz"
        "IGluIHZhcml1cy4gUXVpc3F1ZSB2dWxwdXRhdGUgcG9ydHRpdG9yIG5pc2wsIG5vbiBzY"
        "2"
        "VsZXJp"
        "c3F1ZSBlcmF0IGVsZWlmZW5kIHNlZC4=",
        get_mime_base64_character_set());

    test_base64_encoding(
        "Proin sollicitudin cursus bibendum",
        "UHJvaW4gc29sbGljaXR1ZGluIGN1cnN1cyBiaWJlbmR1bQ==",
        get_mime_base64_character_set());

    test_base64_encoding(
        "Quisque dictum orci in urna cursus maximus",
        "UXVpc3F1ZSBkaWN0dW0gb3JjaSBpbiB1cm5hIGN1cnN1cyBtYXhpbXVz",
        get_mime_base64_character_set());
}

TEST_CASE("missing base64 padding", "[encodings][base64]")
{
    INFO(
        "Testing that the base64 decoder is tolerant of missing padding "
        "characters.");

    REQUIRE(
        base64_decode(
            "UHJvaW4gc29sbGljaXR1ZGluIGN1cnN1cyBiaWJlbmR1bQ",
            get_mime_base64_character_set())
        == string("Proin sollicitudin cursus bibendum"));

    REQUIRE(
        base64_decode(
            "UHJvaW4gc29sbGljaXR1ZGluIGN1cnN1cyBiaWJlbmR1bQ=",
            get_mime_base64_character_set())
        == string("Proin sollicitudin cursus bibendum"));

    REQUIRE(
        base64_decode(
            "TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2Npb"
            "m"
            "cgZWxpdC4",
            get_mime_base64_character_set())
        == string("Lorem ipsum dolor sit amet, consectetur adipiscing elit."));
}

// Test that attempting to base64 decode the given string produces a parsing
// error.
static void
test_malformed_base64(
    string const& malformed_base64, base64_character_set const& character_set)
{
    INFO(
        "Testing that the base64 decoder gracefully handles malformed "
        "base64.");

    CAPTURE(malformed_base64);

    try
    {
        base64_decode(malformed_base64, character_set);
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "base64");
        REQUIRE(
            get_required_error_info<parsed_text_info>(e) == malformed_base64);
    }
}

TEST_CASE("malformed base64", "[encodings][base64]")
{
    // These have an impossible number of characters.
    test_malformed_base64("V", get_mime_base64_character_set());
    test_malformed_base64("ASDFV", get_mime_base64_character_set());

    // These have invalid characters at different positions.
    test_malformed_base64("#SDF", get_mime_base64_character_set());
    test_malformed_base64("A#DF", get_mime_base64_character_set());
    test_malformed_base64("AS#F", get_mime_base64_character_set());
    test_malformed_base64("ASD#", get_mime_base64_character_set());

    // These are decoded against the wrong character set.
    test_malformed_base64("AS-_", get_mime_base64_character_set());
    test_malformed_base64("AS+/", get_url_friendly_base64_character_set());
    test_malformed_base64("1bQ=", get_url_friendly_base64_character_set());
}
