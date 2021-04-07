#include <cradle/thinknode/apm.h>

#include <cradle/core/monitoring.h>
#include <cradle/io/http_requests.hpp>

namespace cradle {

thinknode_app_version_info
get_app_version_info(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& account,
    string const& app,
    string const& version)
{
    auto query = make_get_request(
        session.api_url + "/apm/apps/" + account + "/" + app + "/versions/"
            + version + "?include_manifest=true",
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_dynamic<thinknode_app_version_info>(
        parse_json_response(response));
}

} // namespace cradle
