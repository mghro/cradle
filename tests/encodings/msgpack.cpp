#include <cradle/encodings/msgpack.h>

#include <cstring>

#include <cradle/encodings/json.h>
#include <cradle/utilities/testing.h>
#include <cradle/utilities/text.h>

using namespace cradle;

// Test that some MessagePack data can be translated to and from its expected
// dynamic form.
static void
test_msgpack_encoding(
    uint8_t const* msgpack, size_t size, dynamic const& expected_value)
{
    // Parse it and check that it matches.
    auto converted_value = parse_msgpack_value(msgpack, size);
    REQUIRE(converted_value == expected_value);

    // Also try parsing it as a string.
    auto string_converted_value = parse_msgpack_value(std::string(
        reinterpret_cast<char const*>(msgpack),
        reinterpret_cast<char const*>(msgpack) + size));
    REQUIRE(string_converted_value == expected_value);

    // Convert it back to MessagePack and check that that matches the original.
    auto converted_msgpack = value_to_msgpack_string(converted_value);
    REQUIRE(converted_msgpack.size() == size);
    REQUIRE(std::memcmp(&converted_msgpack[0], msgpack, size) == 0);

    // Also try getting the MessagePack as a blob.
    auto msgpack_blob = value_to_msgpack_blob(converted_value);
    REQUIRE(msgpack_blob.size == size);
    REQUIRE(std::memcmp(msgpack_blob.data, msgpack, size) == 0);
}

TEST_CASE("basic msgpack encoding", "[encodings][msgpack]")
{
    uint8_t const msgpack_data[] = {
        142, 165, 97,  108, 112, 104, 97,  192, 164, 98,  101, 116, 97,  195,
        165, 100, 101, 108, 116, 97,  146, 208, 196, 205, 16,  0,   167, 101,
        112, 115, 105, 108, 111, 110, 146, 203, 191, 248, 0,   0,   0,   0,
        0,   0,   203, 64,  41,  0,   0,   0,   0,   0,   0,   163, 101, 116,
        97,  196, 26,  87,  105, 108, 108, 32,  97,  110, 121, 111, 110, 101,
        32,  101, 118, 101, 114, 32,  115, 101, 101, 32,  116, 104, 105, 115,
        63,  165, 103, 97,  109, 109, 97,  194, 164, 105, 111, 116, 97,  213,
        1,   117, 48,  165, 107, 97,  112, 112, 97,  214, 1,   10,  76,  184,
        0,   166, 108, 97,  109, 98,  100, 97,  215, 1,   0,   0,   0,   220,
        106, 207, 172, 0,   162, 109, 117, 131, 161, 97,  192, 161, 98,  195,
        161, 99,  194, 162, 110, 117, 131, 192, 161, 97,  194, 161, 99,  195,
        161, 98,  165, 116, 104, 101, 116, 97,  212, 1,   100, 162, 120, 105,
        147, 192, 195, 194, 164, 122, 101, 116, 97,  163, 102, 111, 111};
    string json_equivalent =
        R"(
            {
                "alpha": null,
                "beta": true,
                "gamma": false,
                "delta": [ -60, 4096 ],
                "epsilon": [ -1.5, 12.5 ],
                "zeta": "foo",
                "eta": {
                    "type": "base64-encoded-blob",
                    "blob": "V2lsbCBhbnlvbmUgZXZlciBzZWUgdGhpcz8="
                },
                "theta": "1970-01-01T00:00:00.100Z",
                "iota": "1970-01-01T00:00:30.000Z",
                "kappa": "1970-01-03T00:00:00.000Z",
                "lambda": "2000-01-01T00:00:00.000Z",
                "mu": {
                    "a": null,
                    "b": true,
                    "c": false
                },
                "nu": [
                    {
                        "key": null,
                        "value": "a"
                    },
                    {
                        "key": true,
                        "value": "b"
                    },
                    {
                        "key": false,
                        "value": "c"
                    }
                ],
                "xi": [ null, true, false ]
            }
        )";
    test_msgpack_encoding(
        msgpack_data, sizeof(msgpack_data), parse_json_value(json_equivalent));
}

TEST_CASE("custom MessagePack blob ownership", "[encodings][msgpack]")
{
    auto blob = parse_json_value(
        R"(
                {
                    "type": "base64-encoded-blob",
                    "blob": "V2lsbCBhbnlvbmUgZXZlciBzZWUgdGhpcz8="
                }
            )");
    auto msgpack = value_to_msgpack_blob(blob);

    ownership_holder custom_ownership(string("custom"));
    auto parsed_value = parse_msgpack_value(
        custom_ownership,
        reinterpret_cast<uint8_t const*>(msgpack.data),
        msgpack.size);

    auto parsed_blob = cast<cradle::blob>(parsed_value);
    REQUIRE(std::any_cast<string>(parsed_blob.ownership) == "custom");
}

TEST_CASE("unsupported MessagePack extension type", "[encodings][msgpack]")
{
    uint8_t msgpack_data[] = {0xd4, 0x02, 0x00};
    try
    {
        parse_msgpack_value(msgpack_data, 3);
    }
    catch (parsing_error& e)
    {
        REQUIRE(
            get_required_error_info<expected_format_info>(e) == "MessagePack");
        REQUIRE(
            get_required_error_info<parsing_error_info>(e)
            == "unsupported MessagePack extension type");
    }
}

TEST_CASE("malformed MessagePack", "[encodings][msgpack]")
{
    {
        uint8_t msgpack_data[] = {0xd4, 0x01, 0x00};
        REQUIRE_THROWS(parse_msgpack_value(msgpack_data, 2));
    }
    {
        uint8_t msgpack_data[] = {0xc1};
        REQUIRE_THROWS(parse_msgpack_value(msgpack_data, 1));
    }
}

TEST_CASE("blob too large for MessagePack", "[encodings][msgpack]")
{
    try
    {
        value_to_msgpack_string(
            blob{ownership_holder(), nullptr, 0x1'00'00'00'01});
        FAIL("no exception thrown");
    }
    catch (msgpack_blob_size_limit_exceeded& e)
    {
        REQUIRE(
            get_required_error_info<msgpack_blob_size_info>(e)
            == 0x1'00'00'00'01);
        REQUIRE(
            get_required_error_info<msgpack_blob_size_limit_info>(e)
            == 0x1'00'00'00'00);
    }

    // Do another one at exactly the limit just to make sure that it's enforced
    // there. (We would ideally do one right below the limit too, but then that
    // would have to actually be processed.)
    try
    {
        value_to_msgpack_string(
            blob{ownership_holder(), nullptr, 0x1'00'00'00'00});
        FAIL("no exception thrown");
    }
    catch (msgpack_blob_size_limit_exceeded& e)
    {
        REQUIRE(
            get_required_error_info<msgpack_blob_size_info>(e)
            == 0x1'00'00'00'00);
        REQUIRE(
            get_required_error_info<msgpack_blob_size_limit_info>(e)
            == 0x1'00'00'00'00);
    }
}
