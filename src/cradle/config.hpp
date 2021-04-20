#ifndef CRADLE_CONFIG_HPP
#define CRADLE_CONFIG_HPP

#include <cradle/caching/disk_cache.hpp>

namespace cradle {

api(struct)
struct server_config
{
    // config for the disk cache
    omissible<cradle::disk_cache_config> disk_cache;
    // whether or not the server should be open to connections from other
    // machines (defaults to false)
    omissible<bool> open;
    // the WebSocket port on which the server will listen
    omissible<cradle::integer> port;
};

} // namespace cradle

#endif
