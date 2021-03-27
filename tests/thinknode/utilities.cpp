#include <cradle/thinknode/utilities.hpp>

#include <cradle/core/testing.hpp>

using namespace cradle;

TEST_CASE("Thinknode type conversion", "[thinknode][utilities]")
{
    auto tn_named_type = make_thinknode_type_info_with_named_type(
        make_thinknode_named_type_reference(none, "my_app", "my_type"));
    auto named_type = make_api_type_info_with_named_type(
        make_api_named_type_reference("my_app", "my_type"));
    REQUIRE(as_api_type(tn_named_type) == named_type);

    auto tn_integer_type
        = make_thinknode_type_info_with_integer_type(thinknode_integer_type());
    auto integer_type
        = make_api_type_info_with_integer_type(api_integer_type());
    REQUIRE(as_api_type(tn_integer_type) == integer_type);

    auto tn_float_type
        = make_thinknode_type_info_with_float_type(thinknode_float_type());
    auto float_type = make_api_type_info_with_float_type(api_float_type());
    REQUIRE(as_api_type(tn_float_type) == float_type);

    auto tn_string_type
        = make_thinknode_type_info_with_string_type(thinknode_string_type());
    auto string_type = make_api_type_info_with_string_type(api_string_type());
    REQUIRE(as_api_type(tn_string_type) == string_type);

    auto tn_boolean_type
        = make_thinknode_type_info_with_boolean_type(thinknode_boolean_type());
    auto boolean_type
        = make_api_type_info_with_boolean_type(api_boolean_type());
    REQUIRE(as_api_type(tn_boolean_type) == boolean_type);

    auto tn_blob_type
        = make_thinknode_type_info_with_blob_type(thinknode_blob_type());
    auto blob_type = make_api_type_info_with_blob_type(api_blob_type());
    REQUIRE(as_api_type(tn_blob_type) == blob_type);

    auto tn_dynamic_type
        = make_thinknode_type_info_with_dynamic_type(thinknode_dynamic_type());
    auto dynamic_type
        = make_api_type_info_with_dynamic_type(api_dynamic_type());
    REQUIRE(as_api_type(tn_dynamic_type) == dynamic_type);

    auto tn_nil_type
        = make_thinknode_type_info_with_nil_type(thinknode_nil_type());
    auto nil_type = make_api_type_info_with_nil_type(api_nil_type());
    REQUIRE(as_api_type(tn_nil_type) == nil_type);

    auto tn_datetime_type = make_thinknode_type_info_with_datetime_type(
        thinknode_datetime_type());
    auto datetime_type
        = make_api_type_info_with_datetime_type(api_datetime_type());
    REQUIRE(as_api_type(tn_datetime_type) == datetime_type);

    auto tn_array_type = make_thinknode_type_info_with_array_type(
        make_thinknode_array_info(tn_boolean_type, none));
    auto array_type = make_api_type_info_with_array_type(
        make_api_array_info(none, boolean_type));
    REQUIRE(as_api_type(tn_array_type) == array_type);

    auto tn_map_type = make_thinknode_type_info_with_map_type(
        make_thinknode_map_info(tn_array_type, tn_blob_type));
    auto map_type = make_api_type_info_with_map_type(
        make_api_map_info(array_type, blob_type));
    REQUIRE(as_api_type(tn_map_type) == map_type);

    auto tn_struct_type = make_thinknode_type_info_with_structure_type(
        make_thinknode_structure_info(
            {{"def",
              make_thinknode_structure_field_info("ijk", none, tn_array_type)},
             {"abc",
              make_thinknode_structure_field_info(
                  "xyz", true, tn_blob_type)}}));
    auto struct_type
        = make_api_type_info_with_structure_type(api_structure_info(
            {{"def", make_api_structure_field_info("ijk", array_type, none)},
             {"abc", make_api_structure_field_info("xyz", blob_type, true)}}));
    REQUIRE(as_api_type(tn_struct_type) == struct_type);

    auto tn_union_type
        = make_thinknode_type_info_with_union_type(make_thinknode_union_info(
            {{"def", make_thinknode_union_member_info("ijk", tn_array_type)},
             {"abc", make_thinknode_union_member_info("xyz", tn_blob_type)},
             {"ghi",
              make_thinknode_union_member_info("qrs", tn_string_type)}}));
    auto union_type = make_api_type_info_with_union_type(api_union_info(
        {{"def", make_api_union_member_info("ijk", array_type)},
         {"abc", make_api_union_member_info("xyz", blob_type)},
         {"ghi", make_api_union_member_info("qrs", string_type)}}));
    REQUIRE(as_api_type(tn_union_type) == union_type);

    auto tn_optional_type
        = make_thinknode_type_info_with_optional_type(tn_map_type);
    auto optional_type = make_api_type_info_with_optional_type(map_type);
    REQUIRE(as_api_type(tn_optional_type) == optional_type);

    auto tn_enum_type
        = make_thinknode_type_info_with_enum_type(make_thinknode_enum_info(
            {{"def", make_thinknode_enum_value_info("xyz")},
             {"abc", make_thinknode_enum_value_info("qrs")}}));
    auto enum_type = make_api_type_info_with_enum_type(api_enum_info(
        {{"def", make_api_enum_value_info("xyz")},
         {"abc", make_api_enum_value_info("qrs")}}));
    REQUIRE(as_api_type(tn_enum_type) == enum_type);

    auto tn_ref_type
        = make_thinknode_type_info_with_reference_type(tn_named_type);
    auto ref_type = make_api_type_info_with_reference_type(named_type);
    REQUIRE(as_api_type(tn_ref_type) == ref_type);
}

TEST_CASE("Thinknode account name", "[thinknode][utilities]")
{
    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    REQUIRE(get_account_name(session) == "mgh");
}
