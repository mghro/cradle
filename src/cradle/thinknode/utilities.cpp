#include <cradle/thinknode/utilities.h>

#include <cradle/utilities/errors.h>
#include <cradle/utilities/functional.h>

namespace cradle {

api_type_info
as_api_type(thinknode_type_info const& tn)
{
    switch (get_tag(tn))
    {
        case thinknode_type_info_tag::ARRAY_TYPE:
            return make_api_type_info_with_array_type(make_api_array_info(
                as_array_type(tn).size,
                as_api_type(as_array_type(tn).element_schema)));
        case thinknode_type_info_tag::BLOB_TYPE:
            return make_api_type_info_with_blob_type(api_blob_type());
        case thinknode_type_info_tag::BOOLEAN_TYPE:
            return make_api_type_info_with_boolean_type(api_boolean_type());
        case thinknode_type_info_tag::DATETIME_TYPE:
            return make_api_type_info_with_datetime_type(api_datetime_type());
        case thinknode_type_info_tag::DYNAMIC_TYPE:
            return make_api_type_info_with_dynamic_type(api_dynamic_type());
        case thinknode_type_info_tag::ENUM_TYPE:
            return make_api_type_info_with_enum_type(make_api_enum_info(map(
                [](auto const& tn_info) {
                    return make_api_enum_value_info(tn_info.description);
                },
                as_enum_type(tn).values)));
        case thinknode_type_info_tag::FLOAT_TYPE:
            return make_api_type_info_with_float_type(api_float_type());
        case thinknode_type_info_tag::INTEGER_TYPE:
            return make_api_type_info_with_integer_type(api_integer_type());
        case thinknode_type_info_tag::MAP_TYPE:
            return make_api_type_info_with_map_type(make_api_map_info(
                as_api_type(as_map_type(tn).key_schema),
                as_api_type(as_map_type(tn).value_schema)));
        case thinknode_type_info_tag::NAMED_TYPE:
            return make_api_type_info_with_named_type(
                make_api_named_type_reference(
                    as_named_type(tn).app, as_named_type(tn).name));
        case thinknode_type_info_tag::NIL_TYPE:
        default:
            return make_api_type_info_with_nil_type(api_nil_type());
        case thinknode_type_info_tag::OPTIONAL_TYPE:
            return make_api_type_info_with_optional_type(
                as_api_type(as_optional_type(tn)));
        case thinknode_type_info_tag::REFERENCE_TYPE:
            return make_api_type_info_with_reference_type(
                as_api_type(as_reference_type(tn)));
        case thinknode_type_info_tag::STRING_TYPE:
            return make_api_type_info_with_string_type(api_string_type());
        case thinknode_type_info_tag::STRUCTURE_TYPE:
            return make_api_type_info_with_structure_type(
                api_structure_info(map(
                    [](auto const& tn_info) {
                        return make_api_structure_field_info(
                            tn_info.description,
                            as_api_type(tn_info.schema),
                            tn_info.omissible);
                    },
                    as_structure_type(tn).fields)));
        case thinknode_type_info_tag::UNION_TYPE:
            return make_api_type_info_with_union_type(api_union_info(map(
                [](auto const& tn_info) {
                    return make_api_union_member_info(
                        tn_info.description, as_api_type(tn_info.schema));
                },
                as_union_type(tn).members)));
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

thinknode_service_id
get_thinknode_service_id(string const& thinknode_id)
{
    switch (std::stoul(thinknode_id.substr(9, 2), nullptr, 16) >> 2)
    {
        case 1:
            return thinknode_service_id::IAM;
        case 2:
            return thinknode_service_id::APM;
        case 3:
            return thinknode_service_id::ISS;
        case 4:
            return thinknode_service_id::CALC;
        case 5:
            return thinknode_service_id::CAS;
        case 6:
            return thinknode_service_id::RKS;
        case 7:
            return thinknode_service_id::IMMUTABLE;
        default:
            CRADLE_THROW(
                internal_check_failed() << internal_error_message_info(
                    "unrecognized Thinknode service ID"));
    }
}

} // namespace cradle
