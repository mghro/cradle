#include <cradle/websocket/server.hpp>

#include <thread>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <picosha2.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <cradle/disk_cache.hpp>
#include <cradle/encodings/base64.hpp>
#include <cradle/encodings/json.hpp>
#include <cradle/encodings/msgpack.hpp>
#include <cradle/fs/file_io.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/apm.hpp>
#include <cradle/thinknode/calc.hpp>
#include <cradle/thinknode/iam.hpp>
#include <cradle/thinknode/iss.hpp>
#include <cradle/thinknode/utilities.hpp>
#include <cradle/websocket/messages.hpp>

// Include this again because some #defines snuck in to overwrite some of our
// enum constants.
#include <cradle/core/api_types.hpp>

typedef websocketpp::server<websocketpp::config::asio> ws_server_type;

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
        std::lock_guard<std::mutex> lock(queue.mutex);
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
    std::map<connection_hdl, client_connection, std::owner_less<connection_hdl>>
        connections;
    std::mutex mutex;
};

static void
add_client(
    client_connection_list& list,
    connection_hdl hdl,
    client_connection const& client = client_connection())
{
    std::lock_guard<std::mutex> lock(list.mutex);
    list.connections[hdl] = client;
}

static void
remove_client(client_connection_list& list, connection_hdl hdl)
{
    std::lock_guard<std::mutex> lock(list.mutex);
    list.connections.erase(hdl);
}

static client_connection
get_client(client_connection_list& list, connection_hdl hdl)
{
    std::lock_guard<std::mutex> lock(list.mutex);
    return list.connections.at(hdl);
}

template<class Fn>
void
access_client(client_connection_list& list, connection_hdl hdl, Fn const& fn)
{
    std::lock_guard<std::mutex> lock(list.mutex);
    fn(list.connections.at(hdl));
}

template<class Fn>
void
for_each_client(client_connection_list& list, Fn const& fn)
{
    std::lock_guard<std::mutex> lock(list.mutex);
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
    auto json = value_to_json(dynamic);
    websocketpp::lib::error_code ec;
    server.ws.send(hdl, json, websocketpp::frame::opcode::text, ec);
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
                return parse_msgpack_value(data);
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto object
        = retrieve_immutable(connection, session, context_id, immutable_id);

    // Cache the result.
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
    // Try the disk cache.
    auto cache_key = picosha2::hash256_hex_string(
        value_to_msgpack_string(dynamic({"resolve_iss_object_to_immutable",
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
            return *entry->value;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto immutable_id = resolve_iss_object_to_immutable(
        connection, session, context_id, object_id, ignore_upgrades);

    // Cache the result.
    cache.insert(cache_key, immutable_id);

    return immutable_id;
}

static dynamic
get_iss_object(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades = false)
{
    return retrieve_immutable(
        cache,
        connection,
        session,
        context_id,
        resolve_iss_object_to_immutable(
            cache,
            connection,
            session,
            context_id,
            object_id,
            ignore_upgrades));
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
    auto cache_key = picosha2::hash256_hex_string(
        value_to_msgpack_string(dynamic({"get_iss_object_metadata",
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
                return from_dynamic<std::map<string, string>>(
                    parse_msgpack_value(data));
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto metadata
        = get_iss_object_metadata(connection, session, context_id, object_id);

    // Cache the result.
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
    // Try the disk cache.
    auto cache_key
        = picosha2::hash256_hex_string(value_to_msgpack_string(dynamic(
            {"get_app_version_info", session.api_url, account, app, version})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached app version info is stored externally in files.
        if (entry && !entry->value)
        {
            auto data = read_file_contents(cache.get_path_for_id(entry->id));
            if (compute_crc32(data) == entry->crc32)
            {
                return from_dynamic<thinknode_app_version_info>(
                    parse_msgpack_value(data));
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto version_info
        = get_app_version_info(connection, session, account, app, version);

    // Cache the result.
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

    return version_info;
}

thinknode_context_contents
get_context_contents(
    disk_cache& cache,
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id)
{
    // Try the disk cache.
    auto cache_key = picosha2::hash256_hex_string(value_to_msgpack_string(
        dynamic({"get_context_contents", session.api_url, context_id})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached contexts are stored internally, so if the entry exists,
        // there should also be a value.
        if (entry && entry->value)
        {
            return from_dynamic<thinknode_context_contents>(parse_msgpack_value(
                base64_decode(*entry->value, get_mime_base64_character_set())));
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto context_contents
        = get_context_contents(connection, session, context_id);

    // Cache the result.
    cache.insert(
        cache_key,
        base64_encode(
            value_to_msgpack_string(to_dynamic(context_contents)),
            get_mime_base64_character_set()));

    return context_contents;
}

static api_type_info
resolve_named_type_reference(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    api_named_type_reference const& ref)
{
    auto context = get_context_contents(cache, connection, session, context_id);
    for (auto const& app_info : context.contents)
    {
        if (app_info.account == ref.account && app_info.app == ref.app)
        {
            if (!is_version(app_info.source))
            {
                CRADLE_THROW(
                    websocket_server_error() << internal_error_message_info(
                        "apps must be installed as versions"));
            }
            auto version_info = get_app_version_info(
                cache,
                connection,
                session,
                ref.account,
                ref.app,
                as_version(app_info.source));
            for (auto const& type : version_info.manifest->types)
            {
                if (type.name == ref.name)
                {
                    return as_api_type(type.schema);
                }
            }
            CRADLE_THROW(
                websocket_server_error()
                << internal_error_message_info("type not found in app"));
        }
    }
    CRADLE_THROW(
        websocket_server_error()
        << internal_error_message_info("app not found in context"));
}

static string
post_iss_object(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    thinknode_type_info const& schema,
    dynamic const& object)
{
    // Apply type coercion.
    auto coerced_object = coerce_value(
        [&](api_named_type_reference const& ref) {
            return resolve_named_type_reference(
                cache, connection, session, context_id, ref);
        },
        as_api_type(schema),
        object);

    // Try the disk cache.
    auto cache_key = picosha2::hash256_hex_string(
        value_to_msgpack_string(dynamic({"post_iss_object",
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
            return *entry->value;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto object_id = post_iss_object(
        connection, session, context_id, schema, coerced_object);

    // Cache the result.
    cache.insert(cache_key, object_id);

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
        case api_type_info_tag::ARRAY:
            return recurse(as_array(type).element_schema);
        case api_type_info_tag::BLOB:
            return false;
        case api_type_info_tag::BOOLEAN:
            return false;
        case api_type_info_tag::DATETIME:
            return false;
        case api_type_info_tag::DYNAMIC:
            // Technically, there could be a reference stored within a dynamic
            // (or in blobs, strings, etc.), but we're only looking for
            // explicitly typed references.
            return false;
        case api_type_info_tag::ENUM:
            return false;
        case api_type_info_tag::FLOAT:
            return false;
        case api_type_info_tag::INTEGER:
            return false;
        case api_type_info_tag::MAP:
            return recurse(as_map(type).key_schema)
                   || recurse(as_map(type).value_schema);
        case api_type_info_tag::NAMED:
            return recurse(look_up_named_type(as_named(type)));
        case api_type_info_tag::NIL:
        default:
            return false;
        case api_type_info_tag::OPTIONAL:
            return recurse(as_optional(type));
        case api_type_info_tag::REFERENCE:
            return true;
        case api_type_info_tag::STRING:
            return false;
        case api_type_info_tag::STRUCTURE:
            return std::any_of(
                as_structure(type).begin(),
                as_structure(type).end(),
                [&](auto const& pair) { return recurse(pair.second.schema); });
        case api_type_info_tag::UNION:
            return std::any_of(
                as_union(type).begin(),
                as_union(type).end(),
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
        case api_type_info_tag::ARRAY:
            for (auto const& item : cast<dynamic_array>(value))
            {
                recurse(as_array(type).element_schema, item);
            }
            break;
        case api_type_info_tag::BLOB:
            break;
        case api_type_info_tag::BOOLEAN:
            break;
        case api_type_info_tag::DATETIME:
            break;
        case api_type_info_tag::DYNAMIC:
            break;
        case api_type_info_tag::ENUM:
            break;
        case api_type_info_tag::FLOAT:
            break;
        case api_type_info_tag::INTEGER:
            break;
        case api_type_info_tag::MAP:
        {
            auto const& map_type = as_map(type);
            for (auto const& pair : cast<dynamic_map>(value))
            {
                recurse(map_type.key_schema, pair.first);
                recurse(map_type.value_schema, pair.second);
            }
            break;
        }
        case api_type_info_tag::NAMED:
            recurse(look_up_named_type(as_named(type)), value);
            break;
        case api_type_info_tag::NIL:
        default:
            break;
        case api_type_info_tag::OPTIONAL:
        {
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_value_type(map));
            if (tag == "some")
            {
                recurse(as_optional(type), get_field(map, "some"));
            }
            break;
        }
        case api_type_info_tag::REFERENCE:
            visitor(cast<string>(value));
        case api_type_info_tag::STRING:
            break;
        case api_type_info_tag::STRUCTURE:
        {
            auto const& structure_type = as_structure(type);
            auto const& map = cast<dynamic_map>(value);
            for (auto const& pair : structure_type)
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
        case api_type_info_tag::UNION:
        {
            auto const& union_type = as_union(type);
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_value_type(map));
            for (auto const& pair : union_type)
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
    string const& destination_context_id,
    string const& object_id)
{
    // Copying an object requires not just copying the object itself but also
    // any objects that it references. The brute force approach is to download
    // the copied object and scan it for references, recursively copying the
    // referenced objects. We use a slightly less brute force method here by
    // first checking the type of the object to see if it contains any reference
    // types. (If not, we skip the whole download/scan/recurse step.)

    // Note that we apply the recursive step whether or not the object already
    // exists in the destination bucket. It's possible that it was copied
    // improperly (and references objects that haven't been copied), in which
    // case we'll fix it by recursing. (And if it was copied properly, then
    // we'll most likely just hit the cache when we do our redundant recursions,
    // so we don't lose much.)

    // Since we need to query the metadata for the object either way, we try
    // doing it without copying the object first. If that works, then we can
    // skip the copy.
    std::map<string, string> metadata;
    try
    {
        metadata = get_iss_object_metadata(
            cache, connection, session, destination_context_id, object_id);
    }
    catch (...)
    {
        // The object probably actually needs to be copied, so do that and
        // try getting the metadata again.
        copy_iss_object(
            connection,
            session,
            source_bucket,
            destination_context_id,
            object_id);
        metadata = get_iss_object_metadata(
            cache, connection, session, destination_context_id, object_id);
    }

    auto object_type
        = as_api_type(parse_url_type_string(metadata["Thinknode-Type"]));

    auto look_up_named_type = [&](api_named_type_reference const& ref) {
        return resolve_named_type_reference(
            cache, connection, session, destination_context_id, ref);
    };

    if (type_contains_references(look_up_named_type, object_type))
    {
        auto object = get_iss_object(
            cache, connection, session, destination_context_id, object_id);
        visit_references(
            look_up_named_type, object_type, object, [&](string const& ref) {
                copy_iss_object(
                    cache,
                    connection,
                    session,
                    source_bucket,
                    destination_context_id,
                    ref);
            });
    }
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
    auto cache_key = picosha2::hash256_hex_string(value_to_msgpack_string(
        dynamic({"get_calculation_request", session.api_url, calculation_id})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached immutable IDs are stored internally, so if the entry exists,
        // there should also be a value.
        if (entry && entry->value)
        {
            return from_dynamic<calculation_request>(parse_msgpack_value(
                base64_decode(*entry->value, get_mime_base64_character_set())));
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto request = retrieve_calculation_request(
        connection, session, context_id, calculation_id);

    // Cache the result.
    cache.insert(
        cache_key,
        base64_encode(
            value_to_msgpack_string(to_dynamic(request)),
            get_mime_base64_character_set()));

    return request;
}

static string
post_calculation(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& calculation)
{
    // Try the disk cache.
    auto cache_key = picosha2::hash256_hex_string(
        value_to_msgpack_string(dynamic({"post_calculation",
                                         session.api_url,
                                         context_id,
                                         to_dynamic(calculation)})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached immutable IDs are stored internally, so if the entry exists,
        // there should also be a value.
        if (entry && entry->value)
        {
            return *entry->value;
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
    }

    // Query Thinknode.
    auto calculation_id
        = post_calculation(connection, session, context_id, calculation);

    // Cache the result.
    cache.insert(cache_key, calculation_id);

    return calculation_id;
}

struct simple_calculation_submitter : calculation_submission_interface
{
    disk_cache& cache;
    http_connection& connection;

    simple_calculation_submitter(disk_cache& cache, http_connection& connection)
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

static void
process_message(
    websocket_server_impl& server,
    http_connection& connection,
    client_request const& request)
{
    switch (get_tag(request.message))
    {
        case websocket_client_message_tag::REGISTRATION:
        {
            auto const& registration = as_registration(request.message);
            access_client(server.clients, request.client, [&](auto& client) {
                client.name = registration.name;
                client.session = registration.session;
            });
            break;
        }
        case websocket_client_message_tag::TEST:
        {
            websocket_test_response response;
            response.name = get_client(server.clients, request.client).name;
            response.message = as_test(request.message).message;
            send(
                server,
                request.client,
                make_websocket_server_message_with_test(response));
            break;
        }
        case websocket_client_message_tag::CACHE_INSERT:
        {
            auto& insertion = as_cache_insert(request.message);
            server.cache.insert(insertion.key, insertion.value);
            break;
        }
        case websocket_client_message_tag::CACHE_QUERY:
        {
            auto const& key = as_cache_query(request.message);
            auto entry = server.cache.find(key);
            send(
                server,
                request.client,
                make_websocket_server_message_with_cache_response(
                    make_websocket_cache_response(
                        key, entry ? entry->value : none)));
            break;
        }
        case websocket_client_message_tag::GET_ISS_OBJECT:
        {
            auto const& gio = as_get_iss_object(request.message);
            auto object = get_iss_object(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                gio.context_id,
                gio.object_id,
                gio.ignore_upgrades);
            send(
                server,
                request.client,
                make_websocket_server_message_with_get_iss_object_response(
                    get_iss_object_response{gio.request_id,
                                            std::move(object)}));
            break;
        }
        case websocket_client_message_tag::GET_ISS_OBJECT_METADATA:
        {
            auto const& giom = as_get_iss_object_metadata(request.message);
            auto metadata = get_iss_object_metadata(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                giom.context_id,
                giom.object_id);
            send(
                server,
                request.client,
                make_websocket_server_message_with_get_iss_object_metadata_response(
                    get_iss_object_metadata_response{giom.request_id,
                                                     std::move(metadata)}));
            break;
        }
        case websocket_client_message_tag::POST_ISS_OBJECT:
        {
            auto const& pio = as_post_iss_object(request.message);
            auto object_id = post_iss_object(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                pio.context_id,
                parse_url_type_string(pio.schema),
                pio.object);
            send(
                server,
                request.client,
                make_websocket_server_message_with_post_iss_object_response(
                    make_post_iss_object_response(pio.request_id, object_id)));
            break;
        }
        case websocket_client_message_tag::COPY_ISS_OBJECT:
        {
            auto const& cio = as_copy_iss_object(request.message);
            copy_iss_object(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                cio.source_bucket,
                cio.destination_context_id,
                cio.object_id);
            send(
                server,
                request.client,
                make_websocket_server_message_with_copy_iss_object_response(
                    make_copy_iss_object_response(cio.request_id)));
            break;
        }
        case websocket_client_message_tag::GET_CALCULATION_REQUEST:
        {
            auto const& gcr = as_get_calculation_request(request.message);
            auto calc = get_calculation_request(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                gcr.context_id,
                gcr.calculation_id);
            send(
                server,
                request.client,
                make_websocket_server_message_with_get_calculation_request_response(
                    make_get_calculation_request_response(
                        gcr.request_id, calc)));
            break;
        }
        case websocket_client_message_tag::POST_CALCULATION:
        {
            auto const& pc = as_post_calculation(request.message);
            auto calc_id = post_calculation(
                server.cache,
                connection,
                get_client(server.clients, request.client).session,
                pc.context_id,
                pc.calculation);
            send(
                server,
                request.client,
                make_websocket_server_message_with_post_calculation_response(
                    make_post_calculation_response(pc.request_id, calc_id)));
            break;
        }
        case websocket_client_message_tag::RESOLVE_META_CHAIN:
        {
            auto const& rmc = as_resolve_meta_chain(request.message);
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
            send(
                server,
                request.client,
                make_websocket_server_message_with_resolve_meta_chain_response(
                    make_resolve_meta_chain_response(rmc.request_id, calc_id)));
            break;
        }
        case websocket_client_message_tag::KILL:
        {
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
        if (is_kill(request.message))
        {
            break;
        }
        try
        {
            process_message(server, connection, request);
        }
        catch (std::exception& e)
        {
            send(
                server,
                request.client,
                make_websocket_server_message_with_error(e.what()));
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
    websocket_client_message message;
    try
    {
        from_dynamic(&message, parse_json_value(raw_message->get_payload()));
        enqueue_job(server.requests, client_request{hdl, message});
        if (is_kill(message))
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
        std::cerr << "--- error processing message:\n" << e.what() << "\n";
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
                          : make_disk_cache_config(none, 0x1'00'00'00'00));

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
