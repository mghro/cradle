#include <cradle/websocket/local_calcs.h>

#include <thread>

#include <cradle/utilities/testing.h>

#include <cradle/encodings/base64.h>
#include <cradle/utilities/environment.h>
#include <cradle/websocket/messages.hpp>

#include "io/http_requests.hpp"

using namespace cradle;

TEST_CASE("local calcs", "[local_calcs][ws]")
{
    disk_cache cache(disk_cache_config(none, 0x1'00'00'00'00));

    http_request_system http_system;
    http_system.set_cacert_path(some(find_testing_cacert_file()));
    http_connection connection(http_system);

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token
        = get_environment_variable("CRADLE_THINKNODE_API_TOKEN");

    auto eval = [&](calculation_request const& request) {
        return perform_local_calc(
            cache,
            connection,
            session,
            "5dadeb4a004073e81b5e096255e83652",
            request);
    };

    // value
    REQUIRE(
        eval(make_calculation_request_with_value(dynamic{2.5}))
        == dynamic{2.5});
    REQUIRE(
        eval(make_calculation_request_with_value(dynamic{"foobar"}))
        == dynamic{"foobar"});
    REQUIRE(
        eval(make_calculation_request_with_value(dynamic({1.0, true, "x"})))
        == dynamic({1.0, true, "x"}));

    // reference
    REQUIRE(
        eval(make_calculation_request_with_reference(
            "5abd360900c0b14726b4ba1e6e5cdc12"))
        == dynamic(
            {{"demographics",
              {
                  {"birthdate", {{"some", "1800-01-01"}}},
                  {"sex", {{"some", "o"}}},
              }},
             {"medical_record_number", "017-08-01"},
             {"name",
              {{"family_name", "Astroid"},
               {"given_name", "v2"},
               {"middle_name", ""},
               {"prefix", ""},
               {"suffix", ""}}}}));

    // function
    REQUIRE(
        eval(make_calculation_request_with_function(make_function_application(
            "mgh",
            "dosimetry",
            "addition",
            none,
            {make_calculation_request_with_value(dynamic(2.0)),
             make_calculation_request_with_value(dynamic(0.125))})))
        == dynamic(2.125));

    // array
    REQUIRE(
        eval(
            make_calculation_request_with_array(make_calculation_array_request(
                {make_calculation_request_with_value(dynamic(integer(2))),
                 make_calculation_request_with_value(dynamic(integer(0))),
                 make_calculation_request_with_value(dynamic(integer(3)))},
                make_thinknode_type_info_with_integer_type(
                    make_thinknode_integer_type()))))
        == dynamic({integer(2), integer(0), integer(3)}));

    // item
    REQUIRE(
        eval(make_calculation_request_with_item(make_calculation_item_request(
            make_calculation_request_with_value(
                dynamic({integer(2), integer(0), integer(3)})),
            make_calculation_request_with_value(dynamic(integer(1))),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(integer(0)));

    // object
    REQUIRE(
        eval(make_calculation_request_with_object(
            make_calculation_object_request(
                {{"two",
                  make_calculation_request_with_value(dynamic(integer(2)))},
                 {"oh",
                  make_calculation_request_with_value(dynamic(integer(0)))},
                 {"three",
                  make_calculation_request_with_value(dynamic(integer(3)))}},
                make_thinknode_type_info_with_structure_type(
                    make_thinknode_structure_info(
                        {{"two",
                          make_thinknode_structure_field_info(
                              "the two",
                              some(false),
                              make_thinknode_type_info_with_integer_type(
                                  make_thinknode_integer_type()))},
                         {"oh",
                          make_thinknode_structure_field_info(
                              "the oh",
                              some(false),
                              make_thinknode_type_info_with_integer_type(
                                  make_thinknode_integer_type()))},
                         {"three",
                          make_thinknode_structure_field_info(
                              "the three",
                              some(false),
                              make_thinknode_type_info_with_integer_type(
                                  make_thinknode_integer_type()))}})))))
        == dynamic(
            {{"two", integer(2)}, {"oh", integer(0)}, {"three", integer(3)}}));

    // property
    REQUIRE(
        eval(make_calculation_request_with_property(
            make_calculation_property_request(
                make_calculation_request_with_value(dynamic(
                    {{"two", integer(2)},
                     {"oh", integer(0)},
                     {"three", integer(3)}})),
                make_calculation_request_with_value(dynamic("oh")),
                make_thinknode_type_info_with_integer_type(
                    make_thinknode_integer_type()))))
        == dynamic(integer(0)));

    // let/variable
    REQUIRE(
        eval(make_calculation_request_with_let(make_let_calculation_request(
            {{"x", make_calculation_request_with_value(dynamic(integer(2)))}},
            make_calculation_request_with_variable("x"))))
        == dynamic(integer(2)));

    // meta
    REQUIRE(
        eval(make_calculation_request_with_meta(make_meta_calculation_request(
            make_calculation_request_with_value(
                dynamic({{"value", integer(1)}})),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(integer(1)));

    // cast
    REQUIRE(
        eval(make_calculation_request_with_cast(make_calculation_cast_request(
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()),
            make_calculation_request_with_value(dynamic(0.0)))))
        == dynamic(integer(0)));
}
