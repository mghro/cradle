#ifndef CRADLE_THINKNODE_SUPERVISOR_H
#define CRADLE_THINKNODE_SUPERVISOR_H

#include <cradle/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection;

// Execute a local Thinknode calculation by invoking a provider via Docker.
dynamic
supervise_thinknode_calculation(
    http_connection& connection,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image,
    string const& function_name,
    std::vector<dynamic> const& args);

} // namespace cradle

#endif
