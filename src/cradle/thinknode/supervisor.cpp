#include <cradle/thinknode/supervisor.hpp>

#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/ipc.hpp>
#include <cradle/thinknode/messages.hpp>

namespace cradle {

uint8_t static const ipc_version = 1;

dynamic
supervise_thinknode_calculation(
    http_connection& connection,
    thinknode_provider_image_info const& image,
    string const& function_name,
    std::vector<dynamic> const& args)
{
    return dynamic("still not implemented, but closer");
}

} // namespace cradle
