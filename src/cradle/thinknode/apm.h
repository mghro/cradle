#ifndef CRADLE_THINKNODE_APM_H
#define CRADLE_THINKNODE_APM_H

#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection_interface;

// Query a particular version of an app.
thinknode_app_version_info
get_app_version_info(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& account,
    string const& app,
    string const& version);

} // namespace cradle

#endif
