#include <cradle/thinknode/iam.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/io/http_requests.hpp>

namespace cradle {

thinknode_context_contents
get_context_contents(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id)
{
    auto query = make_get_request(
        session.api_url + "/iam/contexts/" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_dynamic<thinknode_context_contents>(
        parse_json_response(response));
}

} // namespace cradle
