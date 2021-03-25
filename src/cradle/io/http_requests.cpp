#include <cradle/io/http_requests.hpp>

#include <cstring>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>

#include <curl/curl.h>

#include <cradle/core/logging.hpp>
#include <cradle/core/monitoring.hpp>
#include <cradle/encodings/json.hpp>
#include <cradle/encodings/msgpack.hpp>
#include <cradle/fs/file_io.hpp>

namespace cradle {

dynamic
parse_json_response(http_response const& response)
{
    return parse_json_value(
        reinterpret_cast<char const*>(response.body.data), response.body.size);
}

dynamic
parse_msgpack_response(http_response const& response)
{
    return parse_msgpack_value(
        reinterpret_cast<uint8_t const*>(response.body.data),
        response.body.size);
}

http_response
make_http_200_response(string body)
{
    http_response response;
    response.status_code = 200;
    response.body = make_string_blob(std::move(body));
    return response;
}

static string
get_method_name(http_request_method method)
{
    return boost::to_upper_copy(string(get_value_id(method)));
};

http_request_system::http_request_system(optional<file_path> cacert_path)
{
    if (curl_global_init(CURL_GLOBAL_ALL))
    {
        CRADLE_THROW(http_request_system_error());
    }
    set_cacert_path(std::move(cacert_path));
}
http_request_system::~http_request_system()
{
    curl_global_cleanup();
}

struct http_connection_impl
{
    CURL* curl;
    optional<file_path> cacert_path;
};

static void
reset_curl_connection(http_connection_impl& connection)
{
    CURL* curl = connection.curl;
    curl_easy_reset(curl);

    // Allow requests to be redirected.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Tell CURL to accept and decode gzipped responses.
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1);

    // Enable SSL verification.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
    if (connection.cacert_path)
    {
        auto path = connection.cacert_path->string();
        curl_easy_setopt(curl, CURLOPT_CAINFO, path.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
}

http_connection::http_connection(http_request_system& system)
{
    impl_ = new http_connection_impl;

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        CRADLE_THROW(http_request_system_error());
    }
    impl_->curl = curl;

    auto cacert_path = system.get_cacert_path();
    // A default cacert file is only necessary on Windows.
    // On other systems, Curl will automatically use the system's certificate
    // file.
#ifdef _WIN32
    if (!cacert_path)
    {
        cacert_path = file_path("cacert.pem");
    }
#endif
    if (cacert_path)
    {
        // Confirm that the file actually exists and can be opened.
        // (Curl will silently ignore it if it can't.)
        std::ifstream in;
        open_file(in, *cacert_path, std::ios::in | std::ios::binary);
    }
    impl_->cacert_path = cacert_path;
}
http_connection::~http_connection()
{
    curl_easy_cleanup(impl_->curl);

    delete impl_;
}

struct send_transmission_state
{
    char const* data = nullptr;
    size_t data_length = 0;
    size_t read_position = 0;
};

static size_t
transmit_request_body(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    send_transmission_state& state
        = *reinterpret_cast<send_transmission_state*>(userdata);
    size_t n_bytes
        = (std::min)(size * nmemb, state.data_length - state.read_position);
    if (n_bytes > 0)
    {
        assert(state.data);
        std::memcpy(ptr, state.data + state.read_position, n_bytes);
        state.read_position += n_bytes;
    }
    return n_bytes;
}

typedef std::unique_ptr<char, decltype(&free)> malloc_buffer_ptr;

struct receive_transmission_state
{
    malloc_buffer_ptr buffer;
    size_t buffer_length = 0;
    size_t write_position = 0;

    receive_transmission_state() : buffer(nullptr, free)
    {
    }
};

static size_t
record_http_response(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    receive_transmission_state& state
        = *reinterpret_cast<receive_transmission_state*>(userdata);
    if (!state.buffer)
    {
        char* allocation = reinterpret_cast<char*>(malloc(4096));
        if (!allocation)
            return 0;
        state.buffer = malloc_buffer_ptr(allocation, free);
        state.buffer_length = 4096;
        state.write_position = 0;
    }

    // Grow the buffer if necessary.
    size_t n_bytes = size * nmemb;
    if (state.buffer_length < (state.write_position + n_bytes))
    {
        // Each time the buffer grows, it doubles in size.
        // This wastes some memory but should be faster in general.
        size_t new_size = state.buffer_length * 2;
        while (new_size < state.buffer_length + n_bytes)
            new_size *= 2;
        char* allocation = reinterpret_cast<char*>(
            realloc(state.buffer.release(), new_size));
        if (!allocation)
            return 0;
        state.buffer = malloc_buffer_ptr(allocation, free);
        state.buffer_length = new_size;
    }

    assert(state.buffer);
    std::memcpy(state.buffer.get() + state.write_position, ptr, n_bytes);
    state.write_position += n_bytes;
    return n_bytes;
}

static void
set_up_send_transmission(
    CURL* curl,
    send_transmission_state& send_state,
    http_request const& request)
{
    send_state.data = request.body.data;
    send_state.data_length = request.body.size;
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, transmit_request_body);
    curl_easy_setopt(curl, CURLOPT_READDATA, &send_state);
}

struct curl_progress_data
{
    check_in_interface* check_in;
    progress_reporter_interface* reporter;
};

static int
curl_progress_callback(
    void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    curl_progress_data* data = reinterpret_cast<curl_progress_data*>(clientp);
    try
    {
        (*data->check_in)();
        (*data->reporter)(
            (dltotal + ultotal == 0)
                ? 0.f
                : float((dlnow + ulnow) / (dltotal + ultotal)));
    }
    catch (...)
    {
        return 1;
    }
    return 0;
}

struct scoped_curl_slist
{
    ~scoped_curl_slist()
    {
        curl_slist_free_all(list);
    }
    curl_slist* list;
};

static blob
make_blob(receive_transmission_state&& transmission)
{
    blob result;
    result.data = transmission.buffer.get();
    result.ownership
        = std::shared_ptr<char>(transmission.buffer.release(), free);
    result.size = transmission.write_position;
    return result;
}

http_request
redact_request(http_request request)
{
    auto authorization_header = request.headers.find("Authorization");
    if (authorization_header != request.headers.end())
        authorization_header->second = "[redacted]";
    return request;
}

http_response
http_connection::perform_request(
    check_in_interface& check_in,
    progress_reporter_interface& reporter,
    http_request const& request)
{
    CRADLE_LOG_CALL(<< CRADLE_LOG_ARG(request))

    CURL* curl = impl_->curl;
    assert(curl);
    reset_curl_connection(*impl_);

    // Set the headers for the request.
    scoped_curl_slist curl_headers;
    curl_headers.list = NULL;
    for (auto const& header : request.headers)
    {
        auto header_string = header.first + ":" + header.second;
        curl_headers.list
            = curl_slist_append(curl_headers.list, header_string.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers.list);

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    if (request.socket)
    {
        curl_easy_setopt(
            curl, CURLOPT_UNIX_SOCKET_PATH, request.socket->c_str());
    }

    // Set up for receiving the response body.
    receive_transmission_state body_receive_state;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, record_http_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_receive_state);

    // Set up for receiving the response headers.
    receive_transmission_state header_receive_state;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, record_http_response);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_receive_state);

    // Let CURL know what the method is and set up for sending the body if
    // necessary.
    send_transmission_state send_state;
    switch (request.method)
    {
        case http_request_method::PUT:
            set_up_send_transmission(curl, send_state, request);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
            curl_easy_setopt(
                curl, CURLOPT_INFILESIZE_LARGE, curl_off_t(request.body.size));
            break;

        case http_request_method::POST:
            set_up_send_transmission(curl, send_state, request);
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(
                curl,
                CURLOPT_POSTFIELDSIZE_LARGE,
                curl_off_t(request.body.size));
            break;

        case http_request_method::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;

        case http_request_method::HEAD:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            break;
    }

    // Set up progress monitoring.
    curl_progress_data progress_data;
    progress_data.check_in = &check_in;
    progress_data.reporter = &reporter;
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &progress_data);

    // Perform the request.
    CURLcode result = curl_easy_perform(curl);

    // Check in again here because if the job was canceled inside the above
    // call, it will just look like an error. We need the cancellation
    // exception to be rethrown.
    check_in();

    // Check for low-level CURL errors.
    if (result != CURLE_OK)
    {
        CRADLE_THROW(
            http_request_failure()
            << attempted_http_request_info(redact_request(request))
            << internal_error_message_info(curl_easy_strerror(result)));
    }

    // Parse the response headers.
    http_header_list response_headers;
    {
        std::istringstream response_header_text(string(
            header_receive_state.buffer.get(),
            header_receive_state.buffer_length));
        string header_line;
        while (std::getline(response_header_text, header_line)
               && header_line != "\r")
        {
            auto index = header_line.find(':', 0);
            if (index != string::npos)
            {
                response_headers[boost::algorithm::trim_copy(
                    header_line.substr(0, index))]
                    = boost::algorithm::trim_copy(
                        header_line.substr(index + 1));
            }
        }
    }

    // Construct the response.
    http_response response;
    response.body = make_blob(std::move(body_receive_state));
    response.headers = std::move(response_headers);
    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response.status_code = boost::numeric_cast<int>(status_code);

    // Check the status code.
    if (status_code < 200 || status_code > 299)
    {
        CRADLE_THROW(
            bad_http_status_code()
            << attempted_http_request_info(redact_request(request))
            << http_response_info(response));
    }

    return response;
}

} // namespace cradle
