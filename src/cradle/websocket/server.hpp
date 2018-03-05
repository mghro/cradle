#ifndef CRADLE_WEBSOCKET_SERVER_HPP
#define CRADLE_WEBSOCKET_SERVER_HPP

#include <cradle/common.hpp>
#include <cradle/config.hpp>

namespace cradle {

CRADLE_DEFINE_EXCEPTION(websocket_server_error)
// This exception provides internal_error_message_info.

// The websocket server uses this type to identify clients.
typedef int websocket_client_id;

struct websocket_server_impl;

struct websocket_server
{
    websocket_server(server_config const& config);
    ~websocket_server();

    void
    listen();

    void
    run();

 private:
    websocket_server_impl* impl_;
};

} // namespace cradle

#endif
