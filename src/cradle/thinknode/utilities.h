#ifndef CRADLE_THINKNODE_UTILITIES_H
#define CRADLE_THINKNODE_UTILITIES_H

#include <cradle/thinknode/types.hpp>

namespace cradle {

api_type_info
as_api_type(thinknode_type_info const& tn);

string
get_account_name(thinknode_session const& session);

// Get the service associated with the given Thinknode ID.
thinknode_service_id
get_thinknode_service_id(string const& thinknode_id);

} // namespace cradle

#endif
