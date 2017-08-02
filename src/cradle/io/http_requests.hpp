#ifndef CRADLE_IO_HTTP_REQUESTS_HPP
#define CRADLE_IO_HTTP_REQUESTS_HPP

#include <cradle/common.hpp>

#include <boost/core/noncopyable.hpp>

// This file defines a low-level facility for doing authenticated HTTP requests.

namespace cradle {

struct progress_reporter_interface;
struct check_in_interface;

// HTTP headers are specified as a mapping from field names to values.
typedef std::map<string,string> http_header_list;

// The body of an HTTP request is a blob.
typedef blob http_body;

// Construct a GET request (in a convenient way).
http_request static inline
make_get_request(string const& url, http_header_list const& headers)
{
    return make_http_request(http_request_method::GET, url, headers, http_body());
}

// Parse a http_response as a JSON value.
value parse_json_response(http_response const& response);

// Parse a http_response as a MessagePack value.
value parse_msgpack_response(http_response const& response);

// This exception indicates a general failure in the HTTP request
// system (e.g., a failure to initialize).
CRADLE_DEFINE_EXCEPTION(http_request_system_error)

// This exception indicates that a failure occurred in the processing
// of a HTTP request that precluded getting a response from the server
// (e.g., the server couldn't be reached).
CRADLE_DEFINE_EXCEPTION(http_request_failure)
// This exception also provides internal_error_message_info.
CRADLE_DEFINE_ERROR_INFO(http_request, attempted_http_request)

// This exception indicates that an HTTP request was resolved but
// resulted in a status code outside the 2xx range. The full response
// is included.
CRADLE_DEFINE_EXCEPTION(bad_http_status_code)
// This exception also provides attempted_http_request_info.
CRADLE_DEFINE_ERROR_INFO(http_response, http_response)

CRADLE_DEFINE_ERROR_INFO(http_request, attempted_http_request)
// This tells whether or not the request failed due to a potentially
// transient cause and is therefore worth retrying.
CRADLE_DEFINE_ERROR_INFO(bool, error_is_transient)

// http_request_system provides global initialization and shutdown of the HTTP request
// system. Exactly one of these objects must be instantiated by the
// application, and its scope must dominate the scope of all other HTTP request
// objects.

struct http_request_system : boost::noncopyable
{
    http_request_system();
    ~http_request_system();
};

// http_connection provides a network connection over which HTTP requests can
// be made.

struct http_connection_interface
{
    // Perform an HTTP request and return the response.
    // Since this may take a long time to complete, monitoring is provided.
    // Accurate progress reporting relies on the web server providing the size
    // of the response.
    virtual http_response
    perform_request(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        http_request const& request) = 0;
};

struct http_connection_impl;

struct http_connection : http_connection_interface, boost::noncopyable
{
    http_connection(http_request_system& system);
    ~http_connection();

    http_response
    perform_request(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        http_request const& request);

 private:
    http_connection_impl* impl_;
};

}

#endif
