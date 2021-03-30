#ifndef CRADLE_WEBSOCKET_LOCAL_CALCS_HPP
#define CRADLE_WEBSOCKET_LOCAL_CALCS_HPP

#include <cradle/caching/disk_cache.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/types.hpp>

namespace cradle {

dynamic
perform_local_calc(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& request);
}

#endif
