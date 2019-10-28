#include <cradle/websocket/local_calcs.hpp>

#include <picosha2.h>

#include <cradle/core/dynamic.hpp>
#include <cradle/encodings/msgpack.hpp>
#include <cradle/fs/file_io.hpp>
#include <cradle/thinknode/supervisor.hpp>
#include <cradle/thinknode/utilities.hpp>

namespace cradle {

// signatures for functions that we're temporarily borrowing from server.cpp:

dynamic
get_iss_object(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades = false);

api_type_info
resolve_named_type_reference(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    api_named_type_reference const& ref);

thinknode_app_version_info
resolve_context_app(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& account,
    string const& app);

// end of temporary borrowing

static uint32_t
compute_crc32(string const& s)
{
    boost::crc_32_type crc;
    crc.process_bytes(s.data(), s.length());
    return crc.checksum();
}

dynamic
perform_local_function_calc(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& account,
    string const& app,
    string const& name,
    std::vector<dynamic> const& args)
{
    // Try the disk cache.
    auto cache_key = picosha2::hash256_hex_string(
        value_to_msgpack_string(dynamic({"local_function_calc",
                                         session.api_url,
                                         context_id,
                                         account,
                                         app,
                                         name,
                                         args})));
    try
    {
        auto entry = cache.find(cache_key);
        // Cached calculation results are stored externally in files.
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

    auto version_info = resolve_context_app(
        cache, connection, session, context_id, account, app);

    auto result = supervise_thinknode_calculation(
        connection,
        account,
        app,
        as_private(*version_info.manifest->provider).image,
        name,
        args);

    // Cache the result.
    auto cache_id = cache.initiate_insert(cache_key);
    auto msgpack = value_to_msgpack_string(result);
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

    return result;
}

dynamic
perform_local_calc(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    std::map<string, dynamic> const& environment,
    calculation_request const& request)
{
    auto recursive_call = [&](calculation_request const& request) {
        return perform_local_calc(
            cache, connection, session, context_id, environment, request);
    };
    auto coercive_call
        = [&](thinknode_type_info const& schema, dynamic const& value) {
              return coerce_value(
                  [&](api_named_type_reference const& ref) {
                      return resolve_named_type_reference(
                          cache, connection, session, context_id, ref);
                  },
                  as_api_type(schema),
                  value);
          };

    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
            return get_iss_object(
                cache, connection, session, context_id, as_reference(request));
        case calculation_request_tag::VALUE:
            return as_value(request);
        case calculation_request_tag::FUNCTION:
            return perform_local_function_calc(
                cache,
                connection,
                session,
                context_id,
                as_function(request).account,
                as_function(request).app,
                as_function(request).name,
                map(recursive_call, as_function(request).args));
        case calculation_request_tag::ARRAY:
            return dynamic(map(
                [&](calculation_request const& item) {
                    return coercive_call(
                        as_array(request).item_schema, recursive_call(item));
                },
                as_array(request).items));
        case calculation_request_tag::ITEM:
            return coercive_call(
                as_item(request).schema,
                cast<dynamic_array>(recursive_call(as_item(request).array))
                    .at(cast<integer>(recursive_call(as_item(request).index))));
        case calculation_request_tag::OBJECT:
        {
            dynamic_map object;
            for (auto const& item : as_object(request).properties)
                object[dynamic(item.first)] = recursive_call(item.second);
            return coercive_call(as_object(request).schema, object);
        }
        case calculation_request_tag::PROPERTY:
            return coercive_call(
                as_property(request).schema,
                cast<dynamic_map>(recursive_call(as_property(request).object))
                    .at(cast<string>(
                        recursive_call(as_property(request).field))));
        case calculation_request_tag::LET:
        {
            std::map<string, dynamic> extended_environment = environment;
            for (auto const& v : as_let(request).variables)
                extended_environment[v.first] = recursive_call(v.second);
            return perform_local_calc(
                cache,
                connection,
                session,
                context_id,
                extended_environment,
                as_let(request).in);
        }
        case calculation_request_tag::VARIABLE:
            return environment.at(as_variable(request));
        case calculation_request_tag::META:
            return coercive_call(
                as_meta(request).schema,
                recursive_call(from_dynamic<calculation_request>(
                    recursive_call(as_meta(request).generator))));
        case calculation_request_tag::CAST:
            return coercive_call(
                as_cast(request).schema,
                recursive_call(as_cast(request).object));
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

dynamic
perform_local_calc(
    disk_cache& cache,
    http_connection& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& request)
{
    return perform_local_calc(
        cache, connection, session, context_id, {}, request);
}

} // namespace cradle
