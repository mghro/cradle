#include <cradle/encodings/json.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>

#include <nlohmann/json.hpp>
#include <simdjson.h>

#include <cradle/encodings/base64.hpp>

namespace cradle {

// JSON I/O

// Check if a JSON array is actually an encoded map.
// This is the case if the array contains only key/value pairs.
static bool
array_resembles_map(simdjson::dom::array const& array)
{
    if (array.size() == 0)
        return false;
    for (auto const& element : array)
    {
        if (element.type() != simdjson::dom::element_type::OBJECT)
            return false;
        simdjson::dom::object object = element;
        if (object.size() != 2
            || object.at_key("key").error() == simdjson::NO_SUCH_FIELD
            || object.at_key("value").error() == simdjson::NO_SUCH_FIELD)
        {
            return false;
        }
    }
    return true;
}

static bool
safe_isdigit(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch));
}

// Read a JSON value into a CRADLE dynamic.
static dynamic
read_json_value(simdjson::dom::element const& json)
{
    switch (json.type())
    {
        case simdjson::dom::element_type::NULL_VALUE:
        default: // to avoid warnings
            return nil;
        case simdjson::dom::element_type::BOOL:
            return bool(json);
        case simdjson::dom::element_type::INT64:
            return boost::numeric_cast<integer>(int64_t(json));
        case simdjson::dom::element_type::UINT64:
            return boost::numeric_cast<integer>(uint64_t(json));
        case simdjson::dom::element_type::DOUBLE:
            return double(json);
        case simdjson::dom::element_type::STRING: {
            // Times are also encoded as JSON strings, so this checks to see if
            // the string parses as a time. If so, it just assumes it's
            // actually a time.
            auto s = json.get_string().value();
            // First check if it looks anything like a time string.
            if (s.length() > 16 && safe_isdigit(s[0]) && safe_isdigit(s[1])
                && safe_isdigit(s[2]) && safe_isdigit(s[3]) && s[4] == '-')
            {
                try
                {
                    auto t = parse_ptime(string(s));
                    // Check that it can be converted back without changing its
                    // value. This could be necessary if we actually expected a
                    // string here.
                    if (to_value_string(t) == s)
                    {
                        return t;
                    }
                }
                catch (...)
                {
                }
            }
            return string(s);
        }
        case simdjson::dom::element_type::ARRAY: {
            simdjson::dom::array source = json;
            // If this resembles an encoded map, read it as that.
            if (array_resembles_map(source))
            {
                dynamic_map map;
                for (auto const& i : source)
                {
                    map[read_json_value(i["key"])]
                        = read_json_value(i["value"]);
                }
                return map;
            }
            // Otherwise, read it as an actual array.
            else
            {
                dynamic_array array;
                array.reserve(source.size());
                for (auto const& i : source)
                {
                    array.push_back(read_json_value(i));
                }
                return array;
            }
        }
        case simdjson::dom::element_type::OBJECT: {
            // An object is analogous to a map, but blobs and references are
            // also encoded as JSON objects, so we have to check here if it's
            // actually one of those.
            simdjson::dom::object object = json;
            auto type = object.at_key("type");
            if (type.error() != simdjson::NO_SUCH_FIELD
                && type.value().is_string()
                && type.value().get_string().value() == "base64-encoded-blob")
            {
                auto json_blob = object.at_key("blob");
                if (json_blob.error() != simdjson::NO_SUCH_FIELD
                    && json_blob.value().is_string())
                {
                    auto encoded = json_blob.value().get_string().value();
                    blob x;
                    size_t decoded_size
                        = get_base64_decoded_length(encoded.length());
                    std::shared_ptr<uint8_t> ptr(
                        new uint8_t[decoded_size], array_deleter<uint8_t>());
                    x.ownership = ptr;
                    x.data = reinterpret_cast<char const*>(ptr.get());
                    base64_decode(
                        ptr.get(),
                        &x.size,
                        encoded.data(),
                        encoded.length(),
                        get_mime_base64_character_set());
                    return x;
                }
                else
                {
                    // This was supposed to be a blob, but it's not.
                    CRADLE_THROW(
                        parsing_error()
                        << expected_format_info("base64-encoded-blob")
                        << parsed_text_info(simdjson::minify(json))
                        << parsing_error_info(
                               "object tagged as blob but missing data"));
                }
            }
            else
            {
                // Otherwise, interpret it as a map.
                dynamic_map map;
                for (auto const& i : object)
                {
                    map[string(i.key)] = read_json_value(i.value);
                }
                return map;
            }
        }
    }
}

dynamic
parse_json_value(char const* json, size_t length)
{
    static simdjson::dom::parser the_parser;
    static std::mutex the_mutex;

    std::lock_guard<std::mutex> guard(the_mutex);

    simdjson::dom::element doc;
    try
    {
        doc = the_parser.parse(json, length);
    }
    catch (std::exception& e)
    {
        CRADLE_THROW(
            parsing_error() << expected_format_info("JSON")
                            << parsed_text_info(string(json, json + length))
                            << parsing_error_info(e.what()));
    }
    return read_json_value(doc);
}

static bool
has_only_string_keys(dynamic_map const& map)
{
    for (auto const& i : map)
    {
        if (i.first.type() != value_type::STRING)
            return false;
    }
    return true;
}

static nlohmann::json
to_nlohmann_json(dynamic const& v)
{
    switch (v.type())
    {
        case value_type::NIL:
        default: // to avoid warnings
            return nullptr;
        case value_type::BOOLEAN:
            return cast<bool>(v);
        case value_type::INTEGER:
            return cast<integer>(v);
        case value_type::FLOAT:
            return cast<double>(v);
        case value_type::STRING:
            return cast<string>(v);
        case value_type::BLOB: {
            blob const& x = cast<blob>(v);
            nlohmann::json json;
            json["type"] = "base64-encoded-blob";
            json["blob"] = base64_encode(
                reinterpret_cast<uint8_t const*>(x.data),
                x.size,
                get_mime_base64_character_set());
            return json;
        }
        case value_type::DATETIME:
            return to_value_string(cast<boost::posix_time::ptime>(v));
        case value_type::ARRAY: {
            nlohmann::json json(nlohmann::json::value_t::array);
            for (auto const& i : cast<dynamic_array>(v))
            {
                json.push_back(to_nlohmann_json(i));
            }
            return json;
        }
        case value_type::MAP: {
            dynamic_map const& x = cast<dynamic_map>(v);
            // If the map has only key strings, encode it directly as a JSON
            // object.
            if (has_only_string_keys(x))
            {
                nlohmann::json json(nlohmann::json::value_t::object);
                for (auto const& i : x)
                {
                    json[cast<string>(i.first)] = to_nlohmann_json(i.second);
                }
                return json;
            }
            // Otherwise, encode it as a array of key/value pairs.
            else
            {
                nlohmann::json json(nlohmann::json::value_t::array);
                for (auto const& i : x)
                {
                    nlohmann::json pair;
                    pair["key"] = to_nlohmann_json(i.first);
                    pair["value"] = to_nlohmann_json(i.second);
                    json.push_back(pair);
                }
                return json;
            }
        }
    }
}

string
value_to_json(dynamic const& v)
{
    auto json = to_nlohmann_json(v);
    return json.dump(4);
}

blob
value_to_json_blob(dynamic const& v)
{
    string json = value_to_json(v);
    blob blob;
    // Don't include the terminating '\0'.
    std::shared_ptr<char> ptr(new char[json.length()], array_deleter<char>());
    blob.ownership = ptr;
    blob.data = ptr.get();
    memcpy(ptr.get(), json.c_str(), json.length());
    blob.size = json.length();
    return blob;
}

} // namespace cradle
