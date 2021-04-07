#include <cradle/thinknode/iss.h>

#include <boost/tokenizer.hpp>

#include <cradle/core/monitoring.h>
#include <cradle/encodings/msgpack.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/calc.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/utilities/text.h>

namespace cradle {

string
resolve_iss_object_to_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades)
{
    auto query = make_get_request(
        session.api_url + "/iss/" + object_id
            + "/immutable?context=" + context_id
            + "&ignore_upgrades=" + (ignore_upgrades ? "true" : "false"),
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    switch (response.status_code)
    {
        case 200: {
            return from_dynamic<id_response>(parse_json_response(response)).id;
        }
        case 202: {
            // The ISS object we're interested in is the result of a
            // calculation that hasn't finished yet, so wait for it to resolve
            // and try again.
            long_poll_calculation_status(
                check_in,
                [](calculation_status const& status) {},
                connection,
                session,
                context_id,
                response.headers["Thinknode-Reference-Id"]);
            return resolve_iss_object_to_immutable(
                connection, session, context_id, object_id, ignore_upgrades);
        }
        case 204: {
            // The ISS object we're interested in is the result of a
            // calculation that failed.
        }
        default: {
            CRADLE_THROW(
                bad_http_status_code()
                << attempted_http_request_info(redact_request(query))
                << http_response_info(response));
        }
    }
}

std::map<string, string>
get_iss_object_metadata(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id)
{
    auto query = make_http_request(
        http_request_method::HEAD,
        session.api_url + "/iss/" + object_id + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token}},
        blob());
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    switch (response.status_code)
    {
        case 200: {
            return response.headers;
        }
        case 202: {
            // The ISS object we're interested in is the result of a
            // calculation that hasn't finished yet, so wait for it to resolve
            // and try again.
            long_poll_calculation_status(
                check_in,
                [](calculation_status const& status) {},
                connection,
                session,
                context_id,
                response.headers["Thinknode-Reference-Id"]);
            return get_iss_object_metadata(
                connection, session, context_id, object_id);
        }
        case 204: {
            // The ISS object we're interested in is the result of a
            // calculation that failed.
        }
        default: {
            CRADLE_THROW(
                bad_http_status_code()
                << attempted_http_request_info(redact_request(query))
                << http_response_info(response));
        }
    }
}

dynamic
retrieve_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& immutable_id)
{
    auto query = make_get_request(
        session.api_url + "/iss/immutable/" + immutable_id
            + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/octet-stream"}});
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return parse_msgpack_response(response);
}

string
get_url_type_string(
    thinknode_session const& session, thinknode_type_info const& schema)
{
    switch (get_tag(schema))
    {
        case thinknode_type_info_tag::ARRAY_TYPE:
            return "array/"
                   + get_url_type_string(
                       session, as_array_type(schema).element_schema);
        case thinknode_type_info_tag::BLOB_TYPE:
            return "blob";
        case thinknode_type_info_tag::BOOLEAN_TYPE:
            return "boolean";
        case thinknode_type_info_tag::DATETIME_TYPE:
            return "datetime";
        case thinknode_type_info_tag::DYNAMIC_TYPE:
            return "dynamic";
        case thinknode_type_info_tag::ENUM_TYPE: {
            std::stringstream ss;
            auto const& e = as_enum_type(schema);
            ss << "enum/" << e.values.size();
            for (auto const& v : e.values)
            {
                ss << "/" << v.first;
            }
            return ss.str();
        }
        case thinknode_type_info_tag::FLOAT_TYPE:
            return "float";
        case thinknode_type_info_tag::INTEGER_TYPE:
            return "integer";
        case thinknode_type_info_tag::MAP_TYPE: {
            auto const& m = as_map_type(schema);
            return "map/" + get_url_type_string(session, m.key_schema) + "/"
                   + get_url_type_string(session, m.value_schema);
        }
        case thinknode_type_info_tag::NAMED_TYPE: {
            auto const& n = as_named_type(schema);
            return "named/"
                   + (n.account ? *n.account : get_account_name(session)) + "/"
                   + n.app + "/" + n.name;
        }
        case thinknode_type_info_tag::NIL_TYPE:
        default:
            return "nil";
        case thinknode_type_info_tag::OPTIONAL_TYPE:
            return "optional/"
                   + get_url_type_string(session, as_optional_type(schema));
        case thinknode_type_info_tag::REFERENCE_TYPE:
            return "reference/"
                   + get_url_type_string(session, as_reference_type(schema));
        case thinknode_type_info_tag::STRING_TYPE:
            return "string";
        case thinknode_type_info_tag::STRUCTURE_TYPE: {
            std::stringstream ss;
            auto const& s = as_structure_type(schema);
            ss << "structure/" << s.fields.size();
            for (auto const& f : s.fields)
            {
                ss << "/" << f.first << "/"
                   << get_url_type_string(session, f.second.schema);
            }
            return ss.str();
        }
        case thinknode_type_info_tag::UNION_TYPE: {
            std::stringstream ss;
            auto const& u = as_union_type(schema);
            ss << "union/" << u.members.size();
            for (auto const& m : u.members)
            {
                ss << "/" << m.first << "/"
                   << get_url_type_string(session, m.second.schema);
            }
            return ss.str();
        }
    }
}

typedef boost::tokenizer<boost::char_separator<char>> tokenizer_t;
typedef tokenizer_t::const_iterator token_iter_t;
typedef std::pair<token_iter_t, token_iter_t> token_range_t;

// Get (and consume) the next from the token range.
// If none is available, an exception is thrown.
static string
get_token(token_range_t& tokens)
{
    if (tokens.first == tokens.second)
        throw "missing type components";
    string token = *tokens.first;
    ++tokens.first;
    return token;
}

static thinknode_type_info
parse_url_type(token_range_t& tokens)
{
    auto type_code = get_token(tokens);
    if (type_code == "array")
    {
        return make_thinknode_type_info_with_array_type(
            make_thinknode_array_info(parse_url_type(tokens), none));
    }
    if (type_code == "blob")
    {
        return make_thinknode_type_info_with_blob_type(thinknode_blob_type());
    }
    if (type_code == "boolean")
    {
        return make_thinknode_type_info_with_boolean_type(
            thinknode_boolean_type());
    }
    if (type_code == "datetime")
    {
        return make_thinknode_type_info_with_datetime_type(
            thinknode_datetime_type());
    }
    if (type_code == "dynamic")
    {
        return make_thinknode_type_info_with_dynamic_type(
            thinknode_dynamic_type());
    }
    if (type_code == "enum")
    {
        auto n_values = lexical_cast<size_t>(get_token(tokens));
        thinknode_enum_info enum_info;
        for (size_t i = 0; i != n_values; ++i)
        {
            enum_info.values[get_token(tokens)] = thinknode_enum_value_info();
        }
        return make_thinknode_type_info_with_enum_type(enum_info);
    }
    if (type_code == "float")
    {
        return make_thinknode_type_info_with_float_type(
            thinknode_float_type());
    }
    if (type_code == "integer")
    {
        return make_thinknode_type_info_with_integer_type(
            thinknode_integer_type());
    }
    if (type_code == "map")
    {
        auto key_schema = parse_url_type(tokens);
        auto value_schema = parse_url_type(tokens);
        return make_thinknode_type_info_with_map_type(
            make_thinknode_map_info(key_schema, value_schema));
    }
    if (type_code == "named")
    {
        auto account = get_token(tokens);
        auto app = get_token(tokens);
        auto name = get_token(tokens);
        return make_thinknode_type_info_with_named_type(
            make_thinknode_named_type_reference(account, app, name));
    }
    if (type_code == "nil")
    {
        return make_thinknode_type_info_with_nil_type(thinknode_nil_type());
    }
    if (type_code == "optional")
    {
        return make_thinknode_type_info_with_optional_type(
            parse_url_type(tokens));
    }
    if (type_code == "reference")
    {
        return make_thinknode_type_info_with_reference_type(
            parse_url_type(tokens));
    }
    if (type_code == "string")
    {
        return make_thinknode_type_info_with_string_type(
            thinknode_string_type());
    }
    if (type_code == "structure")
    {
        auto n_fields = lexical_cast<size_t>(get_token(tokens));
        thinknode_structure_info structure_info;
        for (size_t i = 0; i != n_fields; ++i)
        {
            auto name = get_token(tokens);
            auto schema = parse_url_type(tokens);
            structure_info.fields[name]
                = make_thinknode_structure_field_info("", none, schema);
        }
        return make_thinknode_type_info_with_structure_type(structure_info);
    }
    if (type_code == "union")
    {
        auto n_members = lexical_cast<size_t>(get_token(tokens));
        thinknode_union_info union_info;
        for (size_t i = 0; i != n_members; ++i)
        {
            auto name = get_token(tokens);
            auto schema = parse_url_type(tokens);
            union_info.members[name]
                = make_thinknode_union_member_info("", schema);
        }
        return make_thinknode_type_info_with_union_type(union_info);
    }
    throw "unrecognized type code: " + type_code;
}

thinknode_type_info
parse_url_type_string(string const& url_type)
{
    try
    {
        tokenizer_t tokenizer{url_type, boost::char_separator<char>{"/"}};
        auto tokens = std::make_pair(tokenizer.begin(), tokenizer.end());
        auto type = parse_url_type(tokens);
        if (tokens.first != tokens.second)
        {
            throw "extra type components";
        }
        return type;
    }
    catch (char const* message)
    {
        CRADLE_THROW(
            parsing_error()
            << expected_format_info("Thinknode-style URL type string")
            << parsing_error_info(message) << parsed_text_info(url_type));
    }
}

string
post_iss_object(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    thinknode_type_info const& schema,
    dynamic const& data)
{
    auto query = make_http_request(
        http_request_method::POST,
        session.api_url + "/iss/" + get_url_type_string(session, schema)
            + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"},
         {"Content-Type", "application/octet-stream"}},
        value_to_msgpack_blob(data));
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_dynamic<id_response>(parse_json_response(response)).id;
}

void
copy_iss_object(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& source_bucket,
    string const& destination_context_id,
    string const& object_id)
{
    auto query = make_http_request(
        http_request_method::POST,
        session.api_url + "/iss/" + object_id + "/buckets/" + source_bucket
            + "?context=" + destination_context_id,
        {{"Authorization", "Bearer " + session.access_token}},
        blob());
    null_check_in check_in;
    null_progress_reporter reporter;
    connection.perform_request(check_in, reporter, query);
}

} // namespace cradle
