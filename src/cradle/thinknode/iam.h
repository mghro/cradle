#ifndef CRADLE_THINKNODE_IAM_H
#define CRADLE_THINKNODE_IAM_H

#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection_interface;

// Query the contents of a context.
thinknode_context_contents
get_context_contents(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id);

} // namespace cradle

#endif
