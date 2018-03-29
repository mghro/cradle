#ifndef CRADLE_THINKNODE_ISS_HPP
#define CRADLE_THINKNODE_ISS_HPP

#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection_interface;

// Resolve an ISS object to an immutable ID.
string
resolve_iss_object_to_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades);

// Get the metadata for an ISS object.
std::map<string, string>
get_iss_object_metadata(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id);

// Retrieve an immutable data object.
dynamic
retrieve_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& immutable_id);

// Get the URL form of a schema.
string
get_url_type_string(
    thinknode_session const& session, thinknode_type_info const& schema);

// Parse a schema in URL form.
thinknode_type_info
parse_url_type_string(string const& url_type);

// Post an ISS object and return its ID.
string
post_iss_object(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    thinknode_type_info const& schema,
    dynamic const& data);

// Copy an ISS object from one bucket to another.
void
copy_iss_object(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& source_bucket,
    string const& destination_context_id,
    string const& object_id);

} // namespace cradle

#endif
