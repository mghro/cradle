#include <cradle/thinknode/calc.hpp>

#include <cstring>
#include <fmt/format.h>

#include <fakeit.h>

#include <cradle/core/monitoring.hpp>
#include <cradle/core/testing.h>
#include <cradle/encodings/json.hpp>
#include <cradle/io/http_requests.hpp>

using namespace cradle;
using namespace fakeit;

TEST_CASE("calc status utilities", "[thinknode][tn_calc]")
{
    // We can test most cases in the status ordering and query string
    // translation by simply constructing the expected order of query strings
    // and seeing if that's what repeated application of those functions
    // produces.
    std::vector<string> expected_query_order
        = {"status=waiting",
           "status=queued&queued=pending",
           "status=queued&queued=ready",
           // Do a couple of these manually just to make sure we're not
           // generating the same wrong string as in the actual code.
           "status=calculating&progress=0.00",
           "status=calculating&progress=0.01"};
    for (int i = 2; i != 100; ++i)
    {
        expected_query_order.push_back(
            fmt::format("status=calculating&progress={:4.2f}", i / 100.0));
    }
    expected_query_order.push_back("status=uploading&progress=0.00");
    for (int i = 1; i != 100; ++i)
    {
        expected_query_order.push_back(
            fmt::format("status=uploading&progress={:4.2f}", i / 100.0));
    }
    expected_query_order.push_back("status=completed");
    // Go through the entire progression, starting with the waiting status.
    auto status = some(make_calculation_status_with_waiting(nil));
    for (auto query_string : expected_query_order)
    {
        REQUIRE(calc_status_as_query_string(*status) == query_string);
        status = get_next_calculation_status(*status);
    }
    // Nothing further is possible.
    REQUIRE(!status);

    // Test the other cases that aren't covered above.
    {
        auto failed = make_calculation_status_with_failed(
            calculation_failure_status());
        REQUIRE(get_next_calculation_status(failed) == none);
        REQUIRE(calc_status_as_query_string(failed) == "status=failed");
    }
    {
        auto canceled = make_calculation_status_with_canceled(nil);
        REQUIRE(get_next_calculation_status(canceled) == none);
        REQUIRE(calc_status_as_query_string(canceled) == "status=canceled");
    }
    {
        auto generating = make_calculation_status_with_generating(nil);
        REQUIRE(
            get_next_calculation_status(generating)
            == some(make_calculation_status_with_queued(
                calculation_queue_type::READY)));
        REQUIRE(
            calc_status_as_query_string(generating) == "status=generating");
    }
}

TEST_CASE("calc status query", "[thinknode][tn_calc]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request))
        .Do([&](check_in_interface& check_in,
                progress_reporter_interface& reporter,
                http_request const& request) {
            auto expected_request = make_get_request(
                "https://mgh.thinknode.io/api/v1.0/calc/abc/"
                "status?context=123",
                {{"Authorization", "Bearer xyz"},
                 {"Accept", "application/json"}});
            REQUIRE(request == expected_request);

            return make_http_200_response("{ \"completed\": null }");
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto status = query_calculation_status(
        mock_connection.get(), session, "123", "abc");
    REQUIRE(status == make_calculation_status_with_completed(nil));
}

TEST_CASE("calc request retrieval", "[thinknode][tn_calc]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request))
        .Do([&](check_in_interface& check_in,
                progress_reporter_interface& reporter,
                http_request const& request) {
            auto expected_request = make_get_request(
                "https://mgh.thinknode.io/api/v1.0/calc/abc?context=123",
                {{"Authorization", "Bearer xyz"},
                 {"Accept", "application/json"}});
            REQUIRE(request == expected_request);

            return make_http_200_response("{ \"value\": [2.1, 4.2] }");
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto request = retrieve_calculation_request(
        mock_connection.get(), session, "123", "abc");
    REQUIRE(
        request == make_calculation_request_with_value(dynamic({2.1, 4.2})));
}

TEST_CASE("calc status long polling", "[thinknode][tn_calc]")
{
    Mock<http_connection_interface> mock_connection;

    std::vector<http_request> expected_requests = {
        make_get_request(
            "https://mgh.thinknode.io/api/v1.0/calc/abc/status?context=123",
            {{"Authorization", "Bearer xyz"}, {"Accept", "application/json"}}),
        make_get_request(
            "https://mgh.thinknode.io/api/v1.0/calc/abc/status"
            "?status=calculating&progress=0.12&timeout=120&context=123",
            {{"Authorization", "Bearer xyz"}, {"Accept", "application/json"}}),
        make_get_request(
            "https://mgh.thinknode.io/api/v1.0/calc/abc/status"
            "?status=completed&timeout=120&context=123",
            {{"Authorization", "Bearer xyz"},
             {"Accept", "application/json"}})};

    std::vector<calculation_status> mock_responses
        = {make_calculation_status_with_calculating(
               calculation_calculating_status{0.115}),
           make_calculation_status_with_uploading(
               calculation_uploading_status{0.995}),
           make_calculation_status_with_completed(nil)};

    size_t request_counter = 0;
    When(Method(mock_connection, perform_request))
        .AlwaysDo([&](check_in_interface& check_in,
                      progress_reporter_interface& reporter,
                      http_request const& request) {
            REQUIRE(request == expected_requests.at(request_counter));
            auto response = make_http_200_response(
                value_to_json(to_dynamic(mock_responses.at(request_counter))));
            ++request_counter;
            return response;
        });

    size_t status_counter = 0;
    auto status_checker = [&](calculation_status const& status) {
        REQUIRE(status == mock_responses.at(status_counter));
        ++status_counter;
    };

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    null_check_in check_in;
    long_poll_calculation_status(
        check_in,
        status_checker,
        mock_connection.get(),
        session,
        "123",
        "abc");
    REQUIRE(request_counter == expected_requests.size());
    REQUIRE(status_counter == mock_responses.size());
}

TEST_CASE("calc variable substitution", "[thinknode][tn_calc]")
{
    auto a_substitute = make_calculation_request_with_reference("abc");
    auto b_substitute = make_calculation_request_with_value(dynamic("def"));

    std::map<string, calculation_request> substitutions
        = {{"a", a_substitute}, {"b", b_substitute}};

    auto variable_a = make_calculation_request_with_variable("a");
    auto variable_b = make_calculation_request_with_variable("b");

    auto item_schema
        = make_thinknode_type_info_with_string_type(thinknode_string_type());

    // value
    auto value_calc = make_calculation_request_with_value(dynamic("xyz"));
    REQUIRE(substitute_variables(substitutions, value_calc) == value_calc);

    // reference
    REQUIRE(
        substitute_variables(
            substitutions, make_calculation_request_with_reference("a"))
        == make_calculation_request_with_reference("a"));

    // function
    REQUIRE(
        substitute_variables(
            substitutions,
            make_calculation_request_with_function(make_function_application(
                "my_account",
                "my_name",
                "my_function",
                none,
                {variable_b, value_calc, variable_a})))
        == make_calculation_request_with_function(make_function_application(
            "my_account",
            "my_name",
            "my_function",
            none,
            {b_substitute, value_calc, a_substitute})));

    // array
    auto original_array
        = make_calculation_request_with_array(make_calculation_array_request(
            {variable_a, variable_b, value_calc}, item_schema));
    auto substituted_array
        = make_calculation_request_with_array(make_calculation_array_request(
            {a_substitute, b_substitute, value_calc}, item_schema));
    REQUIRE(
        substitute_variables(substitutions, original_array)
        == substituted_array);
    auto array_schema = make_thinknode_type_info_with_array_type(
        make_thinknode_array_info(item_schema, none));

    // item
    auto original_item
        = make_calculation_request_with_item(make_calculation_item_request(
            original_array,
            make_calculation_request_with_value(dynamic(integer(0))),
            item_schema));
    auto substituted_item
        = make_calculation_request_with_item(make_calculation_item_request(
            substituted_array,
            make_calculation_request_with_value(dynamic(integer(0))),
            item_schema));
    REQUIRE(
        substitute_variables(substitutions, original_item)
        == substituted_item);

    // object
    auto object_schema = make_thinknode_type_info_with_structure_type(
        make_thinknode_structure_info(
            {{"i", make_thinknode_structure_field_info("", none, item_schema)},
             {"j", make_thinknode_structure_field_info("", none, item_schema)},
             {"k",
              make_thinknode_structure_field_info("", none, item_schema)}}));
    auto original_object
        = make_calculation_request_with_object(make_calculation_object_request(
            {{"i", variable_b}, {"j", variable_a}, {"k", value_calc}},
            object_schema));
    auto substituted_object
        = make_calculation_request_with_object(make_calculation_object_request(
            {{"i", b_substitute}, {"j", a_substitute}, {"k", value_calc}},
            object_schema));
    REQUIRE(
        substitute_variables(substitutions, original_object)
        == substituted_object);

    // property
    auto original_property = make_calculation_request_with_property(
        make_calculation_property_request(
            original_object,
            make_calculation_request_with_value(dynamic("j")),
            item_schema));
    auto substituted_property = make_calculation_request_with_property(
        make_calculation_property_request(
            substituted_object,
            make_calculation_request_with_value(dynamic("j")),
            item_schema));
    REQUIRE(
        substitute_variables(substitutions, original_property)
        == substituted_property);

    // let
    REQUIRE_THROWS(substitute_variables(
        substitutions,
        make_calculation_request_with_let(
            make_let_calculation_request(substitutions, value_calc))));

    // variables
    REQUIRE(substitute_variables(substitutions, variable_a) == a_substitute);
    REQUIRE(substitute_variables(substitutions, variable_b) == b_substitute);
    REQUIRE_THROWS(substitute_variables(
        substitutions, make_calculation_request_with_variable("c")));

    // meta
    REQUIRE(
        substitute_variables(
            substitutions,
            make_calculation_request_with_meta(
                make_meta_calculation_request(original_array, array_schema)))
        == make_calculation_request_with_meta(
            make_meta_calculation_request(substituted_array, array_schema)));
}

TEST_CASE("let calculation submission", "[thinknode][tn_calc]")
{
    thinknode_session mock_session;
    mock_session.api_url = "https://mgh.thinknode.io/api/v1.0";
    mock_session.access_token = "xyz";

    string mock_context_id = "abc";

    auto function_call
        = make_calculation_request_with_function(make_function_application(
            "my_account",
            "my_name",
            "my_function",
            none,
            {
                make_calculation_request_with_variable("b"),
                make_calculation_request_with_variable("a"),
            }));

    auto let_calculation
        = make_calculation_request_with_let(make_let_calculation_request(
            {{"a", make_calculation_request_with_value(dynamic("-a-"))},
             {"b", make_calculation_request_with_value(dynamic("-b-"))}},
            make_calculation_request_with_let(make_let_calculation_request(
                {{"c", make_calculation_request_with_value(dynamic("-c-"))},
                 {"d", function_call}},
                make_calculation_request_with_array(
                    make_calculation_array_request(
                        {make_calculation_request_with_variable("a"),
                         make_calculation_request_with_variable("b"),
                         make_calculation_request_with_variable("c"),
                         make_calculation_request_with_variable("d")},
                        make_thinknode_type_info_with_string_type(
                            thinknode_string_type())))))));

    std::vector<calculation_request> expected_requests
        = {make_calculation_request_with_value(dynamic("-a-")),
           make_calculation_request_with_value(dynamic("-b-")),
           make_calculation_request_with_value(dynamic("-c-")),
           make_calculation_request_with_function(make_function_application(
               "my_account",
               "my_name",
               "my_function",
               none,
               {make_calculation_request_with_reference("b-id"),
                make_calculation_request_with_reference("a-id")})),
           make_calculation_request_with_array(make_calculation_array_request(
               {make_calculation_request_with_reference("a-id"),
                make_calculation_request_with_reference("b-id"),
                make_calculation_request_with_reference("c-id"),
                make_calculation_request_with_reference("d-id")},
               make_thinknode_type_info_with_string_type(
                   thinknode_string_type())))};

    std::vector<string> mock_responses
        = {"a-id", "b-id", "c-id", "d-id", "main-id"};

    Mock<calculation_submission_interface> mock_submitter;

    size_t request_counter = 0;
    When(Method(mock_submitter, submit))
        .AlwaysDo(
            [&](thinknode_session const& session,
                string const& context_id,
                calculation_request const& request,
                bool dry_run) -> optional<string> {
                REQUIRE(session == mock_session);
                REQUIRE(context_id == mock_context_id);
                REQUIRE(request == expected_requests.at(request_counter));
                if (!dry_run)
                {
                    auto response = mock_responses.at(request_counter);
                    ++request_counter;
                    return some(response);
                }
                else
                {
                    ++request_counter;
                    return none;
                }
            });

    auto submission_info = submit_let_calculation_request(
        mock_submitter.get(),
        mock_session,
        mock_context_id,
        make_augmented_calculation_request(let_calculation, {"d"}),
        false);
    REQUIRE(request_counter == expected_requests.size());
    REQUIRE(submission_info);
    REQUIRE(submission_info->main_calc_id == "main-id");
    REQUIRE(
        submission_info->reported_subcalcs
        == std::vector<reported_calculation_info>{
            make_reported_calculation_info("d-id", "my_function")});
    REQUIRE(
        submission_info->other_subcalc_ids
        == (std::vector<string>{"a-id", "b-id", "c-id"}));

    request_counter = 0;
    submission_info = submit_let_calculation_request(
        mock_submitter.get(),
        mock_session,
        mock_context_id,
        make_augmented_calculation_request(let_calculation, {"d"}),
        true);
    REQUIRE(!submission_info);
}
