#include <cradle/encodings/json.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>

#include <nlohmann/json.hpp>

#include <cradle/encodings/base64.hpp>

namespace cradle {

// JSON I/O

// Check if a JSON array is actually an encoded map.
// This is the case if the array contains only key/value pairs.
static bool
array_resembles_map(nlohmann::json const& json)
{
    assert(json.is_array());
    for (auto const& element : json)
    {
        if (!element.is_object() || element.size() != 2
            || element.find("key") == element.end()
            || element.find("value") == element.end())
        {
            return false;
        }
    }
    return true;
}

static ptime
parse_time(std::string const& s)
{
    namespace bt = boost::posix_time;
    std::istringstream is(s);
    is.imbue(std::locale(
        std::cout.getloc(), new bt::time_input_facet("%Y-%m-%dT%H:%M:%s")));
    ptime t;
    is >> t;
    char z;
    is.get(z);
    if (t != ptime() && z == 'Z'
        && is.peek() == std::istringstream::traits_type::eof())
    {
        return t;
    }
    CRADLE_THROW(
        parsing_error() << expected_format_info("datetime")
                        << parsed_text_info(s));
}

// Read a JSON value into a CRADLE dynamic.
static dynamic
read_json_value(nlohmann::json const& json)
{
    switch (json.type())
    {
        case nlohmann::json::value_t::null:
        default: // to avoid warnings
            return nil;
        case nlohmann::json::value_t::boolean:
            return json.get<bool>();
        case nlohmann::json::value_t::number_integer:
            return boost::numeric_cast<integer>(json.get<int64_t>());
        case nlohmann::json::value_t::number_unsigned:
            return boost::numeric_cast<integer>(json.get<uint64_t>());
        case nlohmann::json::value_t::number_float:
            return json.get<double>();
        case nlohmann::json::value_t::string:
        {
            // Times are also encoded as JSON strings, so this checks to see if
            // the string parses as a time. If so, it just assumes it's actually
            // a time.
            auto s = json.get<string>();
            // First check if it looks anything like a time string.
            if (s.length() > 16 && isdigit(s[0]) && isdigit(s[1])
                && isdigit(s[2]) && isdigit(s[3]) && s[4] == '-')
            {
                try
                {
                    auto t = parse_time(s);
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
            return s;
        }
        case nlohmann::json::value_t::array:
        {
            // If this resembles an encoded map, read it as that.
            if (!json.empty() && array_resembles_map(json))
            {
                dynamic_map map;
                for (auto const& i : json)
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
                array.reserve(json.size());
                for (auto const& i : json)
                {
                    array.push_back(read_json_value(i));
                }
                return array;
            }
        }
        case nlohmann::json::value_t::object:
        {
            // An object is analagous to a record, but blobs and references are
            // also encoded as JSON objects, so we have to check here if it's
            // actually one of those.
            auto type = json.find("type");
            if (type != json.end() && type->is_string()
                && *type == "base64-encoded-blob")
            {
                auto json_blob = json.find("blob");
                if (json_blob != json.end() && json_blob->is_string())
                {
                    string encoded = *json_blob;
                    blob x;
                    size_t decoded_size
                        = get_base64_decoded_length(encoded.length());
                    std::shared_ptr<uint8_t> ptr(
                        new uint8_t[decoded_size], array_deleter<uint8_t>());
                    x.ownership = ptr;
                    x.data = reinterpret_cast<void const*>(ptr.get());
                    base64_decode(
                        ptr.get(),
                        &x.size,
                        encoded.c_str(),
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
                        << parsed_text_info(json.dump(4))
                        << parsing_error_info(
                               "object tagged as blob but missing data"));
                }
            }
            else
            {
                // Otherwise, interpret it as a record.
                dynamic_map map;
                for (nlohmann::json::const_iterator i = json.begin();
                     i != json.end();
                     ++i)
                {
                    map[i.key()] = read_json_value(i.value());
                }
                return map;
            }
        }
    }
}

dynamic
parse_json_value(char const* json, size_t length)
{
    nlohmann::json parsed_json;
    try
    {
        parsed_json = nlohmann::json::parse(json, json + length);
    }
    catch (std::exception& e)
    {
        CRADLE_THROW(
            parsing_error() << expected_format_info("JSON")
                            << parsed_text_info(string(json, json + length))
                            << parsing_error_info(e.what()));
    }
    return read_json_value(parsed_json);
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

nlohmann::json static to_nlohmann_json(dynamic const& v)
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
        case value_type::BLOB:
        {
            blob const& x = cast<blob>(v);
            nlohmann::json json;
            json["type"] = "base64-encoded-blob";
            json["blob"] = base64_encode(
                static_cast<uint8_t const*>(x.data),
                x.size,
                get_mime_base64_character_set());
            return json;
        }
        case value_type::DATETIME:
            return to_value_string(cast<boost::posix_time::ptime>(v));
        case value_type::ARRAY:
        {
            nlohmann::json json(nlohmann::json::value_t::array);
            for (auto const& i : cast<dynamic_array>(v))
            {
                json.push_back(to_nlohmann_json(i));
            }
            return json;
        }
        case value_type::MAP:
        {
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
