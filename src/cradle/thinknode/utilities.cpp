#include <cradle/thinknode/utilities.hpp>

namespace cradle {

api_type_info
as_api_type(thinknode_type_info const& tn)
{
    switch (get_tag(tn))
    {
        case thinknode_type_info_tag::ARRAY_TYPE:
            return make_api_type_info_with_array(make_api_array_info(
                as_api_type(as_array_type(tn).element_schema),
                as_array_type(tn).size));
        case thinknode_type_info_tag::BLOB_TYPE:
            return make_api_type_info_with_blob(api_blob_type());
        case thinknode_type_info_tag::BOOLEAN_TYPE:
            return make_api_type_info_with_boolean(api_boolean_type());
        case thinknode_type_info_tag::DATETIME_TYPE:
            return make_api_type_info_with_datetime(api_datetime_type());
        case thinknode_type_info_tag::DYNAMIC_TYPE:
            return make_api_type_info_with_dynamic(api_dynamic_type());
        case thinknode_type_info_tag::ENUM_TYPE:
            return make_api_type_info_with_enum(map(
                [](auto const& tn_info) {
                    return make_api_enum_value_info(tn_info.description);
                },
                as_enum_type(tn).values));
        case thinknode_type_info_tag::FLOAT_TYPE:
            return make_api_type_info_with_float(api_float_type());
        case thinknode_type_info_tag::INTEGER_TYPE:
            return make_api_type_info_with_integer(api_integer_type());
        case thinknode_type_info_tag::MAP_TYPE:
            return make_api_type_info_with_map(make_api_map_info(
                as_api_type(as_map_type(tn).key_schema),
                as_api_type(as_map_type(tn).value_schema)));
        case thinknode_type_info_tag::NAMED_TYPE:
            return make_api_type_info_with_named(make_api_named_type_reference(
                as_named_type(tn).account,
                as_named_type(tn).app,
                as_named_type(tn).name));
        case thinknode_type_info_tag::NIL_TYPE:
        default:
            return make_api_type_info_with_nil(api_nil_type());
        case thinknode_type_info_tag::OPTIONAL_TYPE:
            return make_api_type_info_with_optional(
                as_api_type(as_optional_type(tn)));
        case thinknode_type_info_tag::REFERENCE_TYPE:
            return make_api_type_info_with_reference(
                as_api_type(as_reference_type(tn)));
        case thinknode_type_info_tag::STRING_TYPE:
            return make_api_type_info_with_string(api_string_type());
        case thinknode_type_info_tag::STRUCTURE_TYPE:
            return make_api_type_info_with_structure(map(
                [](auto const& tn_info) {
                    return make_api_structure_field_info(
                        tn_info.description,
                        tn_info.omissible,
                        as_api_type(tn_info.schema));
                },
                as_structure_type(tn).fields));
        case thinknode_type_info_tag::UNION_TYPE:
            return make_api_type_info_with_union(map(
                [](auto const& tn_info) {
                    return make_api_union_member_info(
                        tn_info.description, as_api_type(tn_info.schema));
                },
                as_union_type(tn).members));
    }
}

string
get_account_name(thinknode_session const& session)
{
    auto const& url = session.api_url;
    auto start = url.find("://") + 3;
    auto end = url.find(".");
    return url.substr(start, end - start);
}

} // namespace cradle
