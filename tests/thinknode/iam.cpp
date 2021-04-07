#include <cradle/thinknode/iam.h>

#include <fakeit.h>

#include <cradle/core/monitoring.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/utilities/testing.h>

using namespace cradle;
using namespace fakeit;

TEST_CASE("context contents retrieval", "[thinknode][iam]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request))
        .Do([&](check_in_interface& check_in,
                progress_reporter_interface& reporter,
                http_request const& request) {
            auto expected_request = make_get_request(
                "https://mgh.thinknode.io/api/v1.0/iam/contexts/123",
                {{"Authorization", "Bearer xyz"},
                 {"Accept", "application/json"}});
            REQUIRE(request == expected_request);

            return make_http_200_response(
                R"(
                        {
                            "bucket": "hacks",
                            "contents": [
                                {
                                    "account": "outatime",
                                    "app": "grays",
                                    "source": {
                                        "version": "1.0.0"
                                    }
                                },
                                {
                                    "account": "chaom",
                                    "app": "landsraad",
                                    "source": {
                                        "branch": "main"
                                }
                                },
                                {
                                    "account": "wayne_enterprises",
                                    "app": "cellsonar",
                                    "source": {
                                        "commit": "a7e1d608d6ce0c25dc6aa597492a6f09"
                                    }
                                }
                            ]
                        }
                    )");
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto contents
        = get_context_contents(mock_connection.get(), session, "123");
    REQUIRE(
        contents
        == make_thinknode_context_contents(
            "hacks",
            {make_thinknode_context_app_info(
                 "outatime",
                 "grays",
                 make_thinknode_app_source_info_with_version("1.0.0")),
             make_thinknode_context_app_info(
                 "chaom",
                 "landsraad",
                 make_thinknode_app_source_info_with_branch("main")),
             make_thinknode_context_app_info(
                 "wayne_enterprises",
                 "cellsonar",
                 make_thinknode_app_source_info_with_commit(
                     "a7e1d608d6ce0c25dc6aa597492a6f09"))}));
}
