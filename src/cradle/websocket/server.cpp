#include <cradle/io/asio.h>

#include <cradle/websocket/server.hpp>

#include <thread>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#pragma warning(disable : 4701)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <picosha2.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <spdlog/spdlog.h>

#include <cradle/core/diff.hpp>
#include <cradle/core/logging.hpp>
#include <cradle/disk_cache.hpp>
#include <cradle/encodings/base64.hpp>
#include <cradle/encodings/json.hpp>
#include <cradle/encodings/msgpack.hpp>
#include <cradle/encodings/yaml.hpp>
#include <cradle/fs/app_dirs.hpp>
#include <cradle/fs/file_io.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/apm.hpp>
#include <cradle/thinknode/calc.hpp>
#include <cradle/thinknode/iam.hpp>
#include <cradle/thinknode/iss.hpp>
#include <cradle/thinknode/utilities.hpp>
#include <cradle/websocket/local_calcs.hpp>
#include <cradle/websocket/messages.hpp>

// Include this again because some #defines snuck in to overwrite some of our
// enum constants.
#include <cradle/core/api_types.hpp>

struct ws_config : public websocketpp::config::asio
{
    typedef ws_config type;
    typedef websocketpp::config::asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config
    {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::basic_socket::endpoint
            socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config>
        transport_type;

    static const size_t max_message_size = 1000000000;
};

typedef websocketpp::server<ws_config> ws_server_type;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

namespace cradle {
template<class Job>
struct synchronized_job_queue
{
    // the actual job queue
    std::queue<Job> jobs;
    // for controlling access to the job queue
    std::mutex mutex;
    // for signalling when new jobs arrive
    std::condition_variable cv;
};

// Add a job to :queue and notify a waiting thread.
template<class Job>
void
enqueue_job(synchronized_job_queue<Job>& queue, Job&& job)
{
    {
        std::scoped_lock<std::mutex> lock(queue.mutex);
        queue.jobs.push(std::forward<Job>(job));
    }
    queue.cv.notify_one();
}

// Wait until there's at least one job in :queue, then pop and return the
// first job.
template<class Job>
Job
wait_for_job(synchronized_job_queue<Job>& queue)
{
    std::unique_lock<std::mutex> lock(queue.mutex);
    while (queue.jobs.empty())
        queue.cv.wait(lock);
    Job first = std::move(queue.jobs.front());
    queue.jobs.pop();
    return first;
}

struct client_connection
{
    string name;
    thinknode_session session;
};

struct client_connection_list
{
    std::
        map<connection_hdl, client_connection, std::owner_less<connection_hdl>>
            connections;
    std::mutex mutex;
};

static void
add_client(
    client_connection_list& list,
    connection_hdl hdl,
    client_connection const& client = client_connection())
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    list.connections[hdl] = client;
}

static void
remove_client(client_connection_list& list, connection_hdl hdl)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    list.connections.erase(hdl);
}

static client_connection
get_client(client_connection_list& list, connection_hdl hdl)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    return list.connections.at(hdl);
}

template<class Fn>
void
access_client(client_connection_list& list, connection_hdl hdl, Fn const& fn)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    fn(list.connections.at(hdl));
}

template<class Fn>
void
for_each_client(client_connection_list& list, Fn const& fn)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    for (auto& client : list.connections)
    {
        fn(client.first, client.second);
    }
}

struct client_request
{
    connection_hdl client;
    websocket_client_message message;
};

struct websocket_server_impl
{
    server_config config;
    http_request_system http_system;
    ws_server_type ws;
    client_connection_list clients;
    disk_cache cache;
    synchronized_job_queue<client_request> requests;
};

static void
send(
    websocket_server_impl& server,
    connection_hdl hdl,
    websocket_server_message const& message)
{
    auto dynamic = to_dynamic(message);
    auto msgpack = value_to_msgpack_string(dynamic);
    websocketpp::lib::error_code ec;
    server.ws.send(hdl, msgpack, websocketpp::frame::opcode::binary, ec);
    if (ec)
    {
        CRADLE_THROW(
            websocket_server_error()
            << internal_error_message_info(ec.message()));
    }
}

static uint32_t
compute_crc32(string const& s)
{
    boost::crc_32_type crc;
    crc.process_bytes(s.data(), s.length());
    return crc.checksum();
}

static dynamic
retrieve_immutable(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& immutable_id)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(immutable_id));

    // Try the disk cache.
    auto cache_key = picosha2::hash256_hex_string(value_to_msgpack_string(
        dynamic({"retrieve_immutable", session.api_url, immutable_id})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached immutables are stored externally in files.
        if (entry && !entry->value)
        {
            auto data = read_file_contents(cache.get_path_for_id(entry->id));
            if (compute_crc32(data) == entry->crc32)
            {
                spdlog::get("cradle")->info("cache hit on {}", cache_key);
                return parse_msgpack_value(data);
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto object
        = retrieve_immutable(connection, session, context_id, immutable_id);

    // Cache the result.
    try
    {
        auto cache_id = cache.initiate_insert(cache_key);
        auto msgpack = value_to_msgpack_string(object);
        {
            auto entry_path = cache.get_path_for_id(cache_id);
            std::ofstream output;
            open_file(
                output,
                entry_path,
                std::ios::out | std::ios::trunc | std::ios::binary);
            output << msgpack;
        }
        cache.finish_insert(cache_id, compute_crc32(msgpack));
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    return object;
}

static string
resolve_iss_object_to_immutable(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(object_id)
        << CRADLE_LOG_ARG(ignore_upgrades));

    // Try the disk cache.
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"resolve_iss_object_to_immutable",
             session.api_url,
             ignore_upgrades ? "n/a" : context_id,
             object_id})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached immutable IDs are stored internally, so if the entry exists,
        // there should also be a value.
        if (entry && entry->value)
        {
            spdlog::get("cradle")->info("cache hit on {}", cache_key);
            return *entry->value;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto immutable_id = resolve_iss_object_to_immutable(
        connection, session, context_id, object_id, ignore_upgrades);

    // Cache the result.
    try
    {
        cache.insert(cache_key, immutable_id);
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    return immutable_id;
}

static blob
encode_object(output_data_encoding encoding, dynamic const& object)
{
    switch (encoding)
    {
        case output_data_encoding::JSON:
            return value_to_json_blob(object);
        case output_data_encoding::YAML:
            return value_to_yaml_blob(object);
        case output_data_encoding::DIAGNOSTIC_YAML:
            return value_to_diagnostic_yaml_blob(object);
        case output_data_encoding::MSGPACK:
        default:
            return value_to_msgpack_blob(object);
    }
}

dynamic
get_iss_object(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades = false)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(object_id)
        << CRADLE_LOG_ARG(ignore_upgrades));

    auto immutable_id = resolve_iss_object_to_immutable(
        cache, connection, session, context_id, object_id, ignore_upgrades);

    auto object = retrieve_immutable(
        cache, connection, session, context_id, immutable_id);

    return object;
}

static std::map<string, string>
get_iss_object_metadata(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id)
{
    // Try the disk cache.
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"get_iss_object_metadata",
             session.api_url,
             context_id,
             object_id})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached metadata are stored externally in files.
        if (entry && !entry->value)
        {
            auto data = read_file_contents(cache.get_path_for_id(entry->id));
            if (compute_crc32(data) == entry->crc32)
            {
                spdlog::get("cradle")->info("cache hit on {}", cache_key);
                return from_dynamic<std::map<string, string>>(
                    parse_msgpack_value(data));
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto metadata
        = get_iss_object_metadata(connection, session, context_id, object_id);

    // Cache the result.
    try
    {
        auto cache_id = cache.initiate_insert(cache_key);
        auto msgpack = value_to_msgpack_string(to_dynamic(metadata));
        {
            auto entry_path = cache.get_path_for_id(cache_id);
            std::ofstream output;
            open_file(
                output,
                entry_path,
                std::ios::out | std::ios::trunc | std::ios::binary);
            output << msgpack;
        }
        cache.finish_insert(cache_id, compute_crc32(msgpack));
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    return metadata;
}

thinknode_app_version_info
get_app_version_info(
    disk_cache& cache,
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& account,
    string const& app,
    string const& version)
{
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"get_app_version_info",
             session.api_url,
             account,
             app,
             version})));

    static std::unordered_map<string, thinknode_app_version_info> memory_cache;
    auto cache_entry = memory_cache.find(cache_key);
    if (cache_entry != memory_cache.end())
        return cache_entry->second;

    // Try the disk cache.
    try
    {
        auto entry = cache.find(cache_key);
        // Cached app version info is stored externally in files.
        if (entry && !entry->value)
        {
            auto data = read_file_contents(cache.get_path_for_id(entry->id));
            if (compute_crc32(data) == entry->crc32)
            {
                spdlog::get("cradle")->info("disk cache hit on {}", cache_key);
                auto result = from_dynamic<thinknode_app_version_info>(
                    parse_msgpack_value(data));
                memory_cache[cache_key] = result;
                return result;
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto version_info
        = get_app_version_info(connection, session, account, app, version);

    // Cache the result.
    try
    {
        auto cache_id = cache.initiate_insert(cache_key);
        auto msgpack = value_to_msgpack_string(to_dynamic(version_info));
        {
            auto entry_path = cache.get_path_for_id(cache_id);
            std::ofstream output;
            open_file(
                output,
                entry_path,
                std::ios::out | std::ios::trunc | std::ios::binary);
            output << msgpack;
        }
        cache.finish_insert(cache_id, compute_crc32(msgpack));
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    memory_cache[cache_key] = version_info;

    return version_info;
}

thinknode_context_contents
get_context_contents(
    disk_cache& cache,
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id)
{
    // Try the memory cache.
    size_t mem_cache_key = invoke_hash(session.api_url);
    boost::hash_combine(mem_cache_key, invoke_hash(context_id));
    static std::unordered_map<size_t, thinknode_context_contents> memory_cache;
    auto cache_entry = memory_cache.find(mem_cache_key);
    if (cache_entry != memory_cache.end())
        return cache_entry->second;

    // Try the disk cache.
    auto disk_cache_key = picosha2::hash256_hex_string(value_to_msgpack_string(
        dynamic({"get_context_contents", session.api_url, context_id})));
    try
    {
        auto entry = cache.find(disk_cache_key);
        // Cached contexts are stored internally, so if the entry exists,
        // there should also be a value.
        if (entry && entry->value)
        {
            spdlog::get("cradle")->info("cache hit on {}", disk_cache_key);
            auto result = from_dynamic<thinknode_context_contents>(
                parse_msgpack_value(base64_decode(
                    *entry->value, get_mime_base64_character_set())));
            memory_cache[mem_cache_key] = result;
            return result;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", disk_cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", disk_cache_key);

    // Query Thinknode.
    auto context_contents
        = get_context_contents(connection, session, context_id);

    // Cache the result.
    try
    {
        cache.insert(
            disk_cache_key,
            base64_encode(
                value_to_msgpack_string(to_dynamic(context_contents)),
                get_mime_base64_character_set()));
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn(
            "error writing cache entry {}", disk_cache_key);
    }

    memory_cache[mem_cache_key] = context_contents;

    return context_contents;
}

thinknode_app_version_info
resolve_context_app(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& account,
    string const& app)
{
    auto context
        = get_context_contents(cache, connection, session, context_id);
    for (auto const& app_info : context.contents)
    {
        if (app_info.account == account && app_info.app == app)
        {
            if (!is_version(app_info.source))
            {
                CRADLE_THROW(
                    websocket_server_error() << internal_error_message_info(
                        "apps must be installed as versions"));
            }
            return get_app_version_info(
                cache,
                connection,
                session,
                account,
                app,
                as_version(app_info.source));
        }
    }
    CRADLE_THROW(
        websocket_server_error()
        << internal_error_message_info("app not found in context"));
}

api_type_info
resolve_named_type_reference(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    api_named_type_reference const& ref)
{
    // Try the memory cache.
    size_t mem_cache_key = invoke_hash(session.api_url);
    boost::hash_combine(mem_cache_key, invoke_hash(context_id));
    boost::hash_combine(mem_cache_key, invoke_hash(ref));
    static std::unordered_map<size_t, api_type_info> memory_cache;
    auto cache_entry = memory_cache.find(mem_cache_key);
    if (cache_entry != memory_cache.end())
        return cache_entry->second;

    auto version_info = resolve_context_app(
        cache,
        connection,
        session,
        context_id,
        get_account_name(session),
        ref.app);
    for (auto const& type : version_info.manifest->types)
    {
        if (type.name == ref.name)
        {
            auto api_type = as_api_type(type.schema);
            memory_cache[mem_cache_key] = api_type;
            return api_type;
        }
    }
    CRADLE_THROW(
        websocket_server_error()
        << internal_error_message_info("type not found in app"));
}

static string
post_iss_object(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    thinknode_type_info const& schema,
    input_data_encoding encoding,
    blob const& encoded_object)
{
    // Decode the object.
    dynamic decoded_object;
    switch (encoding)
    {
        case input_data_encoding::JSON:
            decoded_object = parse_json_value(
                reinterpret_cast<char const*>(encoded_object.data),
                encoded_object.size);
            break;
        case input_data_encoding::YAML:
            decoded_object = parse_yaml_value(
                reinterpret_cast<char const*>(encoded_object.data),
                encoded_object.size);
            break;
        case input_data_encoding::MSGPACK:
            decoded_object = parse_msgpack_value(
                reinterpret_cast<uint8_t const*>(encoded_object.data),
                encoded_object.size);
            break;
    }

    // Apply type coercion.
    spdlog::get("cradle")->info(
        "coercing " + boost::lexical_cast<string>(decoded_object));
    spdlog::get("cradle")->info(
        "to " + boost::lexical_cast<string>(as_api_type(schema)));
    auto coerced_object = coerce_value(
        [&](api_named_type_reference const& ref) {
            return resolve_named_type_reference(
                cache, connection, session, context_id, ref);
        },
        as_api_type(schema),
        decoded_object);

    // Try the disk cache.
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"post_iss_object",
             session.api_url,
             context_id,
             to_dynamic(schema),
             coerced_object})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached ISS IDs are stored internally, so if the entry exists,
        // there should also be a value.
        if (entry && entry->value)
        {
            spdlog::get("cradle")->info("cache hit on {}", cache_key);
            return *entry->value;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto object_id = post_iss_object(
        connection, session, context_id, schema, coerced_object);

    // Cache the result.
    try
    {
        cache.insert(cache_key, object_id);
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    return object_id;
}

static bool
type_contains_references(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type)
{
    auto recurse = [&look_up_named_type](api_type_info const& type) {
        return type_contains_references(look_up_named_type, type);
    };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY_TYPE:
            return recurse(as_array_type(type).element_schema);
        case api_type_info_tag::BLOB_TYPE:
            return false;
        case api_type_info_tag::BOOLEAN_TYPE:
            return false;
        case api_type_info_tag::DATETIME_TYPE:
            return false;
        case api_type_info_tag::DYNAMIC_TYPE:
            // Technically, there could be a reference stored within a
            // dynamic (or in blobs, strings, etc.), but we're only looking
            // for explicitly typed references.
            return false;
        case api_type_info_tag::ENUM_TYPE:
            return false;
        case api_type_info_tag::FLOAT_TYPE:
            return false;
        case api_type_info_tag::INTEGER_TYPE:
            return false;
        case api_type_info_tag::MAP_TYPE:
            return recurse(as_map_type(type).key_schema)
                   || recurse(as_map_type(type).value_schema);
        case api_type_info_tag::NAMED_TYPE:
            return recurse(look_up_named_type(as_named_type(type)));
        case api_type_info_tag::NIL_TYPE:
        default:
            return false;
        case api_type_info_tag::OPTIONAL_TYPE:
            return recurse(as_optional_type(type));
        case api_type_info_tag::REFERENCE_TYPE:
            return true;
        case api_type_info_tag::STRING_TYPE:
            return false;
        case api_type_info_tag::STRUCTURE_TYPE:
            return std::any_of(
                as_structure_type(type).fields.begin(),
                as_structure_type(type).fields.end(),
                [&](auto const& pair) { return recurse(pair.second.schema); });
        case api_type_info_tag::UNION_TYPE:
            return std::any_of(
                as_union_type(type).members.begin(),
                as_union_type(type).members.end(),
                [&](auto const& pair) { return recurse(pair.second.schema); });
    }
}

void
visit_references(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    dynamic const& value,
    std::function<void(string const& ref)> const& visitor)
{
    auto recurse = [&](api_type_info const& type, dynamic const& value) {
        visit_references(look_up_named_type, type, value, visitor);
    };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY_TYPE:
            for (auto const& item : cast<dynamic_array>(value))
            {
                recurse(as_array_type(type).element_schema, item);
            }
            break;
        case api_type_info_tag::BLOB_TYPE:
            break;
        case api_type_info_tag::BOOLEAN_TYPE:
            break;
        case api_type_info_tag::DATETIME_TYPE:
            break;
        case api_type_info_tag::DYNAMIC_TYPE:
            break;
        case api_type_info_tag::ENUM_TYPE:
            break;
        case api_type_info_tag::FLOAT_TYPE:
            break;
        case api_type_info_tag::INTEGER_TYPE:
            break;
        case api_type_info_tag::MAP_TYPE: {
            auto const& map_type = as_map_type(type);
            for (auto const& pair : cast<dynamic_map>(value))
            {
                recurse(map_type.key_schema, pair.first);
                recurse(map_type.value_schema, pair.second);
            }
            break;
        }
        case api_type_info_tag::NAMED_TYPE:
            recurse(look_up_named_type(as_named_type(type)), value);
            break;
        case api_type_info_tag::NIL_TYPE:
        default:
            break;
        case api_type_info_tag::OPTIONAL_TYPE: {
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_tag(map));
            if (tag == "some")
            {
                recurse(as_optional_type(type), get_field(map, "some"));
            }
            break;
        }
        case api_type_info_tag::REFERENCE_TYPE:
            visitor(cast<string>(value));
        case api_type_info_tag::STRING_TYPE:
            break;
        case api_type_info_tag::STRUCTURE_TYPE: {
            auto const& structure_type = as_structure_type(type);
            auto const& map = cast<dynamic_map>(value);
            for (auto const& pair : structure_type.fields)
            {
                auto const& field_info = pair.second;
                dynamic const* field_value;
                bool field_present = get_field(&field_value, map, pair.first);
                if (field_present)
                {
                    recurse(field_info.schema, *field_value);
                }
            }
            break;
        }
        case api_type_info_tag::UNION_TYPE: {
            auto const& union_type = as_union_type(type);
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_tag(map));
            for (auto const& pair : union_type.members)
            {
                auto const& member_name = pair.first;
                auto const& member_info = pair.second;
                if (tag == member_name)
                {
                    recurse(member_info.schema, get_field(map, member_name));
                }
            }
            break;
        }
    }
}

static void
copy_iss_object(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& source_bucket,
    string const& source_context_id,
    string const& destination_context_id,
    string const& object_id)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(source_context_id)
        << CRADLE_LOG_ARG(destination_context_id) << CRADLE_LOG_ARG(object_id))

    // Maintain a cache of already-copied objects and don't repeat copies.
    auto cache_key
        = source_context_id + "/" + destination_context_id + "/" + object_id;
    static std::unordered_map<string, bool> memory_cache;
    auto cache_entry = memory_cache.find(cache_key);
    if (cache_entry != memory_cache.end())
        return;

    // Copying an object requires not just copying the object itself but
    // also any objects that it references. The brute force approach is to
    // download the copied object and scan it for references, recursively
    // copying the referenced objects. We use a slightly less brute force
    // method here by first checking the type of the object to see if it
    // contains any reference types. (If not, we skip the whole
    // download/scan/recurse step.)

    // Copy the object.
    copy_iss_object(
        connection, session, source_bucket, destination_context_id, object_id);
    auto metadata = get_iss_object_metadata(
        cache, connection, session, source_context_id, object_id);

    auto object_type
        = as_api_type(parse_url_type_string(metadata["Thinknode-Type"]));

    auto look_up_named_type = [&](api_named_type_reference const& ref) {
        return resolve_named_type_reference(
            cache, connection, session, source_context_id, ref);
    };

    if (type_contains_references(look_up_named_type, object_type))
    {
        auto object = get_iss_object(
            cache, connection, session, source_context_id, object_id);
        visit_references(
            look_up_named_type, object_type, object, [&](string const& ref) {
                copy_iss_object(
                    cache,
                    connection,
                    session,
                    source_bucket,
                    source_context_id,
                    destination_context_id,
                    ref);
            });
    }

    memory_cache[cache_key] = true;
}

static calculation_request
get_calculation_request(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calculation_id)
{
    // Try the disk cache.
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"get_calculation_request", session.api_url, calculation_id})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached immutable IDs are stored internally, so if the entry
        // exists, there should also be a value.
        if (entry && entry->value)
        {
            spdlog::get("cradle")->info("cache hit on {}", cache_key);
            return from_dynamic<calculation_request>(
                parse_msgpack_value(base64_decode(
                    *entry->value, get_mime_base64_character_set())));
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto request = retrieve_calculation_request(
        connection, session, context_id, calculation_id);

    // Cache the result.
    try
    {
        cache.insert(
            cache_key,
            base64_encode(
                value_to_msgpack_string(to_dynamic(request)),
                get_mime_base64_character_set()));
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    return request;
}

struct simple_calculation_retriever : calculation_retrieval_interface
{
    disk_cache& cache;
    http_connection& connection;

    simple_calculation_retriever(
        disk_cache& cache, http_connection& connection)
        : cache(cache), connection(connection)
    {
    }

    calculation_request
    retrieve(
        thinknode_session const& session,
        string const& context_id,
        string const& calculation_id)
    {
        return get_calculation_request(
            cache, connection, session, context_id, calculation_id);
    }
};

// Search within a calculation request and return a list of subcalculation
// IDs that match :search_string.
static std::vector<string>
search_calculation(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calculation_id,
    string const& search_string)
{
    simple_calculation_retriever retriever(cache, connection);
    return search_calculation(
        retriever, session, context_id, calculation_id, search_string);
}

static string
post_shallow_calculation(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& calculation)
{
    if (is_reference(calculation))
        return as_reference(calculation);

    // Try the disk cache.
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"post_calculation",
             session.api_url,
             context_id,
             to_dynamic(calculation)})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached immutable IDs are stored internally, so if the entry
        // exists, there should also be a value.
        if (entry && entry->value)
        {
            spdlog::get("cradle")->info("cache hit on {}", cache_key);
            return *entry->value;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on cache entry {}", cache_key);
    }
    spdlog::get("cradle")->info("cache miss on {}", cache_key);

    // Query Thinknode.
    auto calculation_id
        = post_calculation(connection, session, context_id, calculation);

    // Cache the result.
    try
    {
        cache.insert(cache_key, calculation_id);
    }
    catch (...)
    {
        // Something went wrong trying to write the cached value, so issue a
        // warning and move on.
        spdlog::get("cradle")->warn("error writing cache entry {}", cache_key);
    }

    return calculation_id;
}

static calculation_request
shallowly_post_calculation(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& request)
{
    auto recursive_call = [&](calculation_request const& calc) {
        return shallowly_post_calculation(
            cache, connection, session, context_id, calc);
    };
    calculation_request shallow_calc;
    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
        case calculation_request_tag::VALUE:
            return request;
        case calculation_request_tag::FUNCTION:
            shallow_calc = make_calculation_request_with_function(
                make_function_application(
                    as_function(request).account,
                    as_function(request).app,
                    as_function(request).name,
                    as_function(request).level,
                    map(recursive_call, as_function(request).args)));
            break;
        case calculation_request_tag::ARRAY:
            shallow_calc = make_calculation_request_with_array(
                make_calculation_array_request(
                    map(recursive_call, as_array(request).items),
                    as_array(request).item_schema));
            break;
        case calculation_request_tag::ITEM:
            shallow_calc = make_calculation_request_with_item(
                make_calculation_item_request(
                    recursive_call(as_item(request).array),
                    as_item(request).index,
                    as_item(request).schema));
            break;
        case calculation_request_tag::OBJECT:
            shallow_calc = make_calculation_request_with_object(
                make_calculation_object_request(
                    map(recursive_call, as_object(request).properties),
                    as_object(request).schema));
            break;
        case calculation_request_tag::PROPERTY:
            shallow_calc = make_calculation_request_with_property(
                make_calculation_property_request(
                    recursive_call(as_property(request).object),
                    as_property(request).field,
                    as_property(request).schema));
            break;
        case calculation_request_tag::LET:
            shallow_calc = make_calculation_request_with_let(
                make_let_calculation_request(
                    map(recursive_call, as_let(request).variables),
                    as_let(request).in));
            break;
        case calculation_request_tag::VARIABLE:
            return request;
        case calculation_request_tag::META:
            shallow_calc = make_calculation_request_with_meta(
                make_meta_calculation_request(
                    recursive_call(as_meta(request).generator),
                    as_meta(request).schema));
            break;
        case calculation_request_tag::CAST:
            shallow_calc = make_calculation_request_with_cast(
                make_calculation_cast_request(
                    as_cast(request).schema,
                    recursive_call(as_cast(request).object)));
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }

    return make_calculation_request_with_reference(post_shallow_calculation(
        cache, connection, session, context_id, shallow_calc));
}

static string
post_calculation(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& calculation)
{
    auto posted = shallowly_post_calculation(
        cache, connection, session, context_id, calculation);
    return as_reference(posted);
}

struct simple_calculation_submitter : calculation_submission_interface
{
    disk_cache& cache;
    http_connection& connection;

    simple_calculation_submitter(
        disk_cache& cache, http_connection& connection)
        : cache(cache), connection(connection)
    {
    }

    optional<string>
    submit(
        thinknode_session const& session,
        string const& context_id,
        calculation_request const& request,
        bool dry_run)
    {
        // If the calculation is simply a reference, just return the ID
        // directly.
        if (is_reference(request))
            return as_reference(request);

        assert(!dry_run);
        return some(
            post_calculation(cache, connection, session, context_id, request));
    }
};

static string
resolve_meta_chain(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request request)
{
    simple_calculation_submitter submitter(cache, connection);
    while (is_meta(request))
    {
        auto const& generator
            = as_meta(request)
                  .generator; // Should be moved out to avoid copies.
        auto submission_info = submit_let_calculation_request(
            submitter,
            session,
            context_id,
            augmented_calculation_request{generator, {}});
        request = from_dynamic<calculation_request>(get_iss_object(
            cache,
            connection,
            session,
            context_id,
            submission_info->main_calc_id));
    }
    auto submission_info = submit_let_calculation_request(
        submitter,
        session,
        context_id,
        augmented_calculation_request{std::move(request), {}});
    return submission_info->main_calc_id;
}

static bool
is_iss_id(dynamic const& value)
{
    if (value.type() == value_type::STRING)
    {
        auto const& id = cast<string>(value);
        if (id.length() == 32)
        {
            try
            {
                return get_thinknode_service_id(id)
                       == thinknode_service_id::ISS;
            }
            catch (...)
            {
            }
        }
    }
    return false;
}

static object_tree_diff
compute_iss_tree_diff(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id_a,
    string const& object_id_a,
    string const& context_id_b,
    string const& object_id_b)
{
    object_tree_diff tree_diff;

    auto object_a = get_iss_object(
        cache, connection, session, context_id_a, object_id_a);
    auto object_b = get_iss_object(
        cache, connection, session, context_id_b, object_id_b);
    auto diff = compute_value_diff(object_a, object_b);

    value_diff relevant_diff;
    for (auto& item : diff)
    {
        if (item.a && is_iss_id(*item.a) && item.b && is_iss_id(*item.b))
        {
            auto subtree_diff = compute_iss_tree_diff(
                cache,
                connection,
                session,
                context_id_a,
                cast<string>(*item.a),
                context_id_b,
                cast<string>(*item.b));
            for (auto& node : subtree_diff)
            {
                node.path_from_root.insert(
                    node.path_from_root.begin(),
                    item.path.begin(),
                    item.path.end() - 1);
            }
            std::move(
                subtree_diff.begin(),
                subtree_diff.end(),
                std::back_inserter(tree_diff));
            continue;
        }

        relevant_diff.push_back(std::move(item));
    }

    if (!relevant_diff.empty())
    {
        object_node_diff node_diff;
        node_diff.service = thinknode_service_id::ISS;
        node_diff.path_from_root = value_diff_path();
        node_diff.id_in_a = object_id_a;
        node_diff.id_in_b = object_id_b;
        node_diff.diff = std::move(relevant_diff);
        tree_diff.push_back(std::move(node_diff));
    }

    return tree_diff;
}

static object_tree_diff
compute_calc_tree_diff(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id_a,
    string const& calc_id_a,
    string const& context_id_b,
    string const& calc_id_b)
{
    object_tree_diff tree_diff;

    auto calc_a = get_calculation_request(
        cache, connection, session, context_id_a, calc_id_a);
    auto calc_b = get_calculation_request(
        cache, connection, session, context_id_b, calc_id_b);
    auto diff = compute_value_diff(to_dynamic(calc_a), to_dynamic(calc_b));

    value_diff relevant_diff;
    for (auto& item : diff)
    {
        if (item.op == value_diff_op::UPDATE && !item.path.empty()
            && item.path.back() == "reference")
        {
            auto id_a = cast<string>(*item.a);
            auto id_b = cast<string>(*item.b);

            auto service_a = get_thinknode_service_id(id_a);
            auto service_b = get_thinknode_service_id(id_b);

            object_tree_diff subtree_diff;

            if (service_a == thinknode_service_id::CALC
                && service_b == thinknode_service_id::CALC)
            {
                subtree_diff = compute_calc_tree_diff(
                    cache,
                    connection,
                    session,
                    context_id_a,
                    id_a,
                    context_id_b,
                    id_b);
            }
            else
            {
                subtree_diff = compute_iss_tree_diff(
                    cache,
                    connection,
                    session,
                    context_id_a,
                    id_a,
                    context_id_b,
                    id_b);
            }
            for (auto& node : subtree_diff)
            {
                node.path_from_root.insert(
                    node.path_from_root.begin(),
                    item.path.begin(),
                    item.path.end() - 1);
            }
            std::move(
                subtree_diff.begin(),
                subtree_diff.end(),
                std::back_inserter(tree_diff));
            continue;
        }

        relevant_diff.push_back(std::move(item));
    }

    if (!relevant_diff.empty())
    {
        object_node_diff node_diff;
        node_diff.service = thinknode_service_id::CALC;
        node_diff.path_from_root = value_diff_path();
        node_diff.id_in_a = calc_id_a;
        node_diff.id_in_b = calc_id_b;
        node_diff.diff = std::move(relevant_diff);
        tree_diff.push_back(std::move(node_diff));
    }

    return tree_diff;
}

static void
send_response(
    websocket_server_impl& server,
    client_request const& request,
    server_message_content const& content)
{
    send(
        server,
        request.client,
        make_websocket_server_message(request.message.request_id, content));
}

static void
process_message(
    websocket_server_impl& server,
    http_connection& connection,
    client_request const& request)
{
    CRADLE_LOG_CALL(<< CRADLE_LOG_ARG(request.message))

    auto const& content = request.message.content;
    switch (get_tag(content))
    {
        case client_message_content_tag::REGISTRATION: {
            auto const& registration = as_registration(content);
            access_client(server.clients, request.client, [&](auto& client) {
                client.name = registration.name;
                client.session = registration.session;
            });
            break;
        }
        case client_message_content_tag::TEST: {
            websocket_test_response response;
            response.name = get_client(server.clients, request.client).name;
            response.message = as_test(content).message;
            send_response(
                server,
                request,
                make_server_message_content_with_test(response));
            break;
        }
        case client_message_content_tag::CACHE_INSERT: {
            auto& insertion = as_cache_insert(content);
            server.cache.insert(insertion.key, insertion.value);
            break;
        }
        case client_message_content_tag::CACHE_QUERY: {
            auto const& key = as_cache_query(content);
            auto entry = server.cache.find(key);
            send_response(
                server,
                request,
                make_server_message_content_with_cache_response(
                    make_websocket_cache_response(
                        key, entry ? entry->value : none)));
            break;
        }
        case client_message_content_tag::ISS_OBJECT: {
            auto const& gio = as_iss_object(content);
            auto object = get_iss_object(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                gio.context_id,
                gio.object_id,
                gio.ignore_upgrades);
            auto encoded_object = encode_object(gio.encoding, object);
            send_response(
                server,
                request,
                make_server_message_content_with_iss_object_response(
                    iss_object_response{std::move(encoded_object)}));
            break;
        }
        case client_message_content_tag::RESOLVE_ISS_OBJECT: {
            auto const& rio = as_resolve_iss_object(content);
            auto immutable_id = resolve_iss_object_to_immutable(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                rio.context_id,
                rio.object_id,
                rio.ignore_upgrades);
            send_response(
                server,
                request,
                make_server_message_content_with_resolve_iss_object_response(
                    resolve_iss_object_response{immutable_id}));
            break;
        }
        case client_message_content_tag::ISS_OBJECT_METADATA: {
            auto const& giom = as_iss_object_metadata(content);
            auto metadata = get_iss_object_metadata(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                giom.context_id,
                giom.object_id);
            send_response(
                server,
                request,
                make_server_message_content_with_iss_object_metadata_response(
                    iss_object_metadata_response{std::move(metadata)}));
            break;
        }
        case client_message_content_tag::POST_ISS_OBJECT: {
            auto const& pio = as_post_iss_object(content);
            auto object_id = post_iss_object(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                pio.context_id,
                parse_url_type_string(pio.schema),
                pio.encoding,
                pio.object);
            send_response(
                server,
                request,
                make_server_message_content_with_post_iss_object_response(
                    make_post_iss_object_response(object_id)));
            break;
        }
        case client_message_content_tag::COPY_ISS_OBJECT: {
            auto const& cio = as_copy_iss_object(content);
            auto source_bucket
                = get_context_contents(
                      server.cache,
                      connection,
                      get_client(server.clients, request.client).session,
                      cio.source_context_id)
                      .bucket;
            copy_iss_object(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                source_bucket,
                cio.source_context_id,
                cio.destination_context_id,
                cio.object_id);
            send_response(
                server,
                request,
                make_server_message_content_with_copy_iss_object_response(
                    nil));
            break;
        }
        case client_message_content_tag::CALCULATION_REQUEST: {
            auto const& gcr = as_calculation_request(content);
            auto calc = get_calculation_request(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                gcr.context_id,
                gcr.calculation_id);
            send_response(
                server,
                request,
                make_server_message_content_with_calculation_request_response(
                    make_calculation_request_response(calc)));
            break;
        }
        case client_message_content_tag::CALCULATION_DIFF: {
            auto const& cdr = as_calculation_diff(content);
            auto diff = compute_calc_tree_diff(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                cdr.context_a,
                cdr.id_a,
                cdr.context_b,
                cdr.id_b);
            send_response(
                server,
                request,
                make_server_message_content_with_calculation_diff_response(
                    diff));
            break;
        }
        case client_message_content_tag::ISS_DIFF: {
            auto const& idr = as_iss_diff(content);
            auto diff = compute_iss_tree_diff(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                idr.context_a,
                idr.id_a,
                idr.context_b,
                idr.id_b);
            send_response(
                server,
                request,
                make_server_message_content_with_iss_diff_response(diff));
            break;
        }
        case client_message_content_tag::CALCULATION_SEARCH: {
            auto const& csr = as_calculation_search(content);
            auto matches = search_calculation(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                csr.context_id,
                csr.calculation_id,
                csr.search_string);
            send_response(
                server,
                request,
                make_server_message_content_with_calculation_search_response(
                    make_calculation_search_response(matches)));
            break;
        }
        case client_message_content_tag::POST_CALCULATION: {
            auto const& pc = as_post_calculation(content);
            auto calc_id = post_calculation(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                pc.context_id,
                pc.calculation);
            send_response(
                server,
                request,
                make_server_message_content_with_post_calculation_response(
                    make_post_calculation_response(calc_id)));
            break;
        }
        case client_message_content_tag::RESOLVE_META_CHAIN: {
            auto const& rmc = as_resolve_meta_chain(content);
            auto calc_id = resolve_meta_chain(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                rmc.context_id,
                make_calculation_request_with_meta(meta_calculation_request{
                    std::move(rmc.generator),
                    // This isn't used.
                    make_thinknode_type_info_with_dynamic_type(
                        thinknode_dynamic_type())}));
            send_response(
                server,
                request,
                make_server_message_content_with_resolve_meta_chain_response(
                    make_resolve_meta_chain_response(calc_id)));
            break;
        }
        case client_message_content_tag::PERFORM_LOCAL_CALC: {
            auto const& pc = as_perform_local_calc(content);
            auto result = perform_local_calc(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                pc.context_id,
                pc.calculation);
            send_response(
                server,
                request,
                make_server_message_content_with_local_calc_result(result));
            break;
        }
        case client_message_content_tag::KILL: {
            break;
        }
    }
}

static void
process_messages(websocket_server_impl& server)
{
    http_connection connection(server.http_system);
    while (1)
    {
        auto request = wait_for_job(server.requests);
        if (is_kill(request.message.content))
        {
            break;
        }
        try
        {
            process_message(server, connection, request);
        }
        catch (bad_http_status_code& e)
        {
            spdlog::get("cradle")->error(e.what());
            send_response(
                server,
                request,
                make_server_message_content_with_error(
                    make_error_response_with_bad_status_code(
                        make_http_failure_info(
                            get_required_error_info<
                                attempted_http_request_info>(e),
                            get_required_error_info<http_response_info>(e)))));
        }
        catch (std::exception& e)
        {
            spdlog::get("cradle")->error(e.what());
            send_response(
                server,
                request,
                make_server_message_content_with_error(
                    make_error_response_with_unknown(e.what())));
        }
    }
}

static void
on_open(websocket_server_impl& server, connection_hdl hdl)
{
    add_client(server.clients, hdl);
}

static void
on_close(websocket_server_impl& server, connection_hdl hdl)
{
    remove_client(server.clients, hdl);
}

static void
on_message(
    websocket_server_impl& server,
    connection_hdl hdl,
    ws_server_type::message_ptr raw_message)
{
    string request_id;
    try
    {
        auto dynamic_message = parse_msgpack_value(raw_message->get_payload());
        request_id = cast<string>(
            get_field(cast<dynamic_map>(dynamic_message), "request_id"));
        websocket_client_message message;
        from_dynamic(&message, dynamic_message);
        enqueue_job(server.requests, client_request{hdl, message});
        if (is_kill(message.content))
        {
            for_each_client(
                server.clients,
                [&](connection_hdl hdl, client_connection const& client) {
                    server.ws.close(
                        hdl, websocketpp::close::status::going_away, "killed");
                });
            server.ws.stop();
        }
    }
    catch (std::exception& e)
    {
        spdlog::get("cradle")->error("error processing message: {}", e.what());
        send(
            server,
            hdl,
            make_websocket_server_message(
                request_id,
                make_server_message_content_with_error(
                    make_error_response_with_unknown(e.what()))));
    }
}

static void
initialize(websocket_server_impl& server, server_config const& config)
{
    server.config = config;

    if (config.cacert_file)
        server.http_system.set_cacert_path(
            some(file_path(*config.cacert_file)));

    server.cache.reset(
        config.disk_cache ? *config.disk_cache
                          : disk_cache_config(none, 0x1'00'00'00'00));

    server.ws.clear_access_channels(websocketpp::log::alevel::all);
    server.ws.init_asio();
    server.ws.set_open_handler(
        [&](connection_hdl hdl) { on_open(server, hdl); });
    server.ws.set_close_handler(
        [&](connection_hdl hdl) { on_close(server, hdl); });
    server.ws.set_message_handler(
        [&](connection_hdl hdl, ws_server_type::message_ptr message) {
            on_message(server, hdl, message);
        });

    // Create and register the logger.
    if (!spdlog::get("cradle"))
    {
        std::vector<spdlog::sink_ptr> sinks;
#ifdef _WIN32
        sinks.push_back(
            std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());
#else
        sinks.push_back(
            std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>());
#endif
        auto log_path = get_user_logs_dir(none, "cradle") / "log";
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path.string(), 262144, 2));
        auto combined_logger = std::make_shared<spdlog::logger>(
            "cradle", begin(sinks), end(sinks));
        spdlog::register_logger(combined_logger);
    }
}

websocket_server::websocket_server(server_config const& config)
{
    impl_ = new websocket_server_impl;
    initialize(*impl_, config);
}

websocket_server::~websocket_server()
{
    delete impl_;
}

void
websocket_server::listen()
{
    auto& server = *impl_;
    bool open = server.config.open ? *server.config.open : false;
    auto port = server.config.port ? *server.config.port : 41071;
    if (open)
    {
        server.ws.listen(port);
    }
    else
    {
        server.ws.listen(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), port));
    }
    server.ws.start_accept();
}

void
websocket_server::run()
{
    auto& server = *impl_;

    // Start a thread to process messages.
    std::thread processing_thread([&]() { process_messages(server); });

    server.ws.run();

    processing_thread.join();
}

} // namespace cradle
