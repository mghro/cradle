#ifndef CRADLE_WEBSOCKET_MESSAGES_HPP
#define CRADLE_WEBSOCKET_MESSAGES_HPP

#include <cradle/core/diff.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/types.hpp>

namespace cradle {

api(struct)
struct websocket_registration_message
{
    std::string name;
    cradle::thinknode_session session;
};

api(struct)
struct websocket_test_query
{
    std::string message;
};

api(struct)
struct websocket_test_response
{
    std::string name;
    std::string message;
};

api(struct)
struct websocket_cache_insert
{
    std::string key;
    std::string value;
};

api(struct)
struct websocket_cache_response
{
    std::string key;
    optional<std::string> value;
};

api(enum)
enum class output_data_encoding
{
    // YAML
    YAML,
    // Diagnostic YAML - This is designed to be more human-friendly and will
    // omit large values (like blobs) that seem meaningless to humans.
    DIAGNOSTIC_YAML,
    // MessagePack
    MSGPACK,
    // JSON
    JSON
};

api(enum)
enum class input_data_encoding
{
    // YAML
    YAML,
    // MessagePack
    MSGPACK,
    // JSON
    JSON
};

api(struct)
struct iss_object_request
{
    std::string context_id;
    std::string object_id;
    bool ignore_upgrades;
    cradle::output_data_encoding encoding;
};

api(struct)
struct iss_object_response
{
    cradle::blob object;
};

api(struct)
struct resolve_iss_object_request
{
    std::string context_id;
    std::string object_id;
    bool ignore_upgrades;
};

api(struct)
struct resolve_iss_object_response
{
    std::string immutable_id;
};

api(struct)
struct iss_object_metadata_request
{
    std::string context_id;
    std::string object_id;
};

api(struct)
struct iss_object_metadata_response
{
    std::map<std::string, std::string> metadata;
};

api(struct)
struct post_iss_object_request
{
    std::string context_id;
    std::string schema;
    cradle::input_data_encoding encoding;
    cradle::blob object;
};

api(struct)
struct post_iss_object_response
{
    std::string object_id;
};

api(struct)
struct copy_iss_object_request
{
    std::string source_context_id;
    std::string destination_context_id;
    std::string object_id;
};

api(struct)
struct post_calculation_request
{
    std::string context_id;
    cradle::calculation_request calculation;
};

api(struct)
struct post_calculation_response
{
    std::string calculation_id;
};

api(struct)
struct calculation_search_request
{
    std::string context_id;
    std::string calculation_id;
    std::string search_string;
};

api(struct)
struct calculation_search_response
{
    std::vector<std::string> matches;
};

api(struct)
struct resolve_meta_chain_request
{
    std::string context_id;
    cradle::calculation_request generator;
};

api(struct)
struct resolve_meta_chain_response
{
    std::string calculation_id;
};

api(struct)
struct calculation_request_message
{
    std::string context_id;
    std::string calculation_id;
};

api(struct)
struct calculation_request_response
{
    cradle::calculation_request calculation;
};

api(struct)
struct calculation_diff_request
{
    std::string id_a;
    std::string context_a;
    std::string id_b;
    std::string context_b;
};

api(struct)
struct object_node_diff
{
    // the service that this node is from (ISS or calc)
    cradle::thinknode_service_id service;
    // the path from the root object to the node being diffed
    cradle::value_diff_path path_from_root;
    // ID of the node in the 'a' object
    std::string id_in_a;
    // ID of the node in the 'b' object
    std::string id_in_b;
    // diff within this node
    cradle::value_diff diff;
};

typedef std::vector<cradle::object_node_diff> object_tree_diff;

typedef cradle::object_tree_diff calculation_diff_response;

api(struct)
struct iss_diff_request
{
    std::string id_a;
    std::string context_a;
    std::string id_b;
    std::string context_b;
};

typedef cradle::object_tree_diff iss_diff_response;

api(union)
union client_message_content
{
    cradle::nil_t kill;
    cradle::websocket_registration_message registration;
    cradle::websocket_test_query test;
    cradle::websocket_cache_insert cache_insert;
    std::string cache_query;
    cradle::iss_object_request iss_object;
    cradle::resolve_iss_object_request resolve_iss_object;
    cradle::iss_object_metadata_request iss_object_metadata;
    cradle::post_iss_object_request post_iss_object;
    cradle::copy_iss_object_request copy_iss_object;
    cradle::post_calculation_request post_calculation;
    cradle::resolve_meta_chain_request resolve_meta_chain;
    cradle::calculation_request_message calculation_request;
    cradle::calculation_diff_request calculation_diff;
    cradle::calculation_search_request calculation_search;
    cradle::iss_diff_request iss_diff;
    cradle::post_calculation_request perform_local_calc;
};

api(struct)
struct websocket_client_message
{
    std::string request_id;
    cradle::client_message_content content;
};

api(struct)
struct http_failure_info
{
    cradle::http_request attempted_request;
    cradle::http_response response;
};

api(union)
union error_response
{
    // an HTTP request returned a bad status code
    http_failure_info bad_status_code;
    std::string unknown;
};

api(union)
union server_message_content
{
    cradle::websocket_test_response test;
    cradle::websocket_cache_response cache_response;
    cradle::error_response error;
    cradle::iss_object_response iss_object_response;
    cradle::resolve_iss_object_response resolve_iss_object_response;
    cradle::iss_object_metadata_response iss_object_metadata_response;
    cradle::post_iss_object_response post_iss_object_response;
    cradle::nil_t copy_iss_object_response;
    cradle::post_calculation_response post_calculation_response;
    cradle::resolve_meta_chain_response resolve_meta_chain_response;
    cradle::calculation_request_response calculation_request_response;
    cradle::calculation_diff_response calculation_diff_response;
    cradle::calculation_search_response calculation_search_response;
    cradle::iss_diff_response iss_diff_response;
    cradle::dynamic local_calc_result;
};

api(struct)
struct websocket_server_message
{
    std::string request_id;
    cradle::server_message_content content;
};

} // namespace cradle

#endif
