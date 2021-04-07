#include <cradle/api_index.hpp>
#include <cradle/core/api_types.hpp>
#include <cradle/encodings/base64.h>
#include <cradle/encodings/json.h>

#include <picosha2.h>

#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>

namespace cradle {

string
generate_function_uid(
    string const& name,
    std::vector<api_function_parameter_info> const& parameters,
    unsigned revision)
{
    api_function_uid_contents uid(name, parameters, revision);
    auto json = value_to_json(to_dynamic(uid));

    return base64_encode(
        picosha2::hash256_hex_string(json), get_mime_base64_character_set());
}

void
register_api_mutation_type(
    api_implementation& api,
    string const& description,
    string const& upgrade_version,
    string const& upgrade_type,
    string const& body)
{
    api_upgrade_type_info up;
    up.name = upgrade_type + string("_") + upgrade_version;
    up.description = description;

    up.schema
        = make_upgrade_type_info_with_mutation_type(parse_json_value(body));
    api
    .upgrades.push_back(up);
}

void
register_api_named_type(
    api_implementation& api,
    string const& name,
    unsigned version,
    string const& description,
    api_type_info const& info,
    upgrade_type upgrade)
{
    api
    .types.push_back(
        api_named_type_implementation_info(name, description, upgrade, info));
}

void
register_api_record_type(
    api_implementation& api,
    string const& record_name,
    string const& description,
    string const& account,
    string const& app,
    string const& name)
{
    api_named_record_type_info rti;
    rti.description = description;
    rti.name = record_name;

    api_record_named_type_info rni;
    if (account.size() > 1)
    {
        rni.account = account;
    }
    if (app.size() > 1)
    {
        rni.app = app;
    }
    rni.name = name;

    api_record_info ri;
    ri.schema.named_type = rni;

    rti.schema = make_api_type_info_with_record_type(ri);

    api
    .records.push_back(rti);
}

// api_upgrade_type_info
// make_upgrade_function_api_info(api_function_ptr const& f)
// {
//     if (!is_function_type(f->api_info.schema))
//     {
//         throw exception(
//             "Upgrade api info found that is not a function: "
//             + f->api_info.name);
//     }
//     auto fs = as_function_type(f->api_info.schema);
//     if (!is_named_type(fs.returns.schema))
//     {
//         throw exception(
//             "Upgrade api info found that doesn't return a named type: "
//             + f->api_info.name);
//     }
//     auto return_type = as_named_type(fs.returns.schema);
//     api_upgrade_type_info up;
//     up.name = "upgrade_" + to_string(return_type.name) + string("_")
//               + f->implementation_info.upgrade_version;
//     up.description = "Upgrade: " + f->api_info.description;
//     api_upgrade_function_info upfi;
//     upfi.function = f->api_info.name;
//     upfi.type = return_type.name;
//     upfi.version = f->implementation_info.upgrade_version;
//     up.schema = make_upgrade_type_info_with_upgrade_type(upfi);

//     return up;
// }

// void
// register_api_function(api_implementation& api, api_function_ptr const& f)
// {
//     string const& uid = f->implementation_info.uid;
//     auto existing = api
//     .functions.find(uid);
//     if (existing != api.functions.end())
//     {
//         throw exception(
//             "duplicate function UID detected:\n" + uid + "\n"
//             + existing->second->api_info.name + "\n" + f->api_info.name);
//     }
//     api
//     .functions[uid] = f;

//     if (is_upgrade(f->implementation_info))
//     {
//         api
//         .upgrades.push_back(make_upgrade_function_api_info(f));
//     }
// }

// bool
// function_is_upgrade(
//     std::vector<api_upgrade_type_info> const& upgrades, string
//     function_name)
// {
//     bool is_upgrade = false;
//     for (auto const& upgrade : upgrades)
//     {
//         if (upgrade.schema.type == upgrade_type_info_type::UPGRADE_TYPE)
//         {
//             auto ug = as_upgrade_type(upgrade.schema);
//             if (ug.function == function_name)
//             {
//                 is_upgrade = true;
//                 break;
//             }
//         }
//     }
//     return is_upgrade;
// }

// api_named_type_info
// remove_upgrade_info_from_named_type(
//     api_named_type_implementation_info const& ut)
// {
//     return api_named_type_info(ut.name, ut.description, ut.schema);
// }

// std::vector<api_named_type_info>
// get_api_named_type_documentation_definition(api_implementation const& api)
// {
//     std::vector<api_named_type_info> api_types;
//     for_each(
//         api.types.begin(),
//         api.types.end(),
//         [&](api_named_type_implementation_info const& ut) {
//             api_types.push_back(remove_upgrade_info_from_named_type(ut));
//         });
//     return api_types;
// }

// api_documentation
// get_api_documentation(
//     api_implementation const& api, bool include_upgrade_functions)
// {
//     std::vector<api_function_info> function_info;
//     for (auto const& f : api.functions)
//     {
//         if (!include_upgrade_functions
//             || function_is_upgrade(api.upgrades, f.second->api_info.name))
//         {
//             function_info.push_back(f.second->api_info);
//         }
//     }
//     return api_documentation(
//         get_api_named_type_documentation_definition(api),
//         function_info,
//         generate_api_upgrades(api),
//         api.records);
// }

// string
// get_api_implementation_documentation(api_implementation const& api)
// {
//     std::vector<api_function_info> function_info;
//     for (auto const& f : api.functions)
//     {
//         if (function_is_upgrade(api.upgrades, f.second->api_info.name))
//         {
//             function_info.push_back(f.second->api_info);
//         }
//     }
//     return value_to_json(to_dynamic(api.provider));
// }

// string
// get_manifest_json(api_implementation const& api)
// {
//     return value_to_json(to_dynamic(get_api_documentation(api, false)));
// }

// string
// get_manifest_json_with_upgrades(api_implementation const& api)
// {
//     return value_to_json(to_dynamic(get_api_documentation(api, true)));
// }

// api_documentation
// get_api_upgrade_documentation(api_implementation const& api)
// {
//     std::vector<api_function_info> function_info;
//     for (auto const& f : api.functions)
//     {
//         if (function_is_upgrade(api.upgrades, f.second->api_info.name))
//         {
//             function_info.push_back(f.second->api_info);
//         }
//     }
//     return api_documentation(
//         get_api_named_type_documentation_definition(api),
//         function_info,
//         generate_api_upgrades(api),
//         api.dependencies,
//         api.provider,
//         api.records);
// }

// std::vector<api_upgrade_type_info>
// generate_api_upgrades(api_implementation const& api)
// {
//     std::vector<api_upgrade_type_info> upgrades;

//     for (auto const& st : api.types)
//     {
//         if (st.upgrade == upgrade_type::FUNCTION)
//         {
//             api_upgrade_type_info up;
//             up.name = string("upgrade_value_") + st.name;
//             up.description = string("upgrade for type ") + st.name;
//             api_upgrade_function_info up_fun;
//             up_fun.function = string("upgrade_value_") + st.name;
//             up_fun.type = st.name;
//             up_fun.version = api
//             .previous_release_version.version;
//             up.schema = make_upgrade_type_info_with_upgrade_type(up_fun);
//             upgrades.push_back(up);
//         }
//     }

//     return upgrades;
// }

// api_function_interface const&
// find_function_by_name(api_implementation const& api, string const& name)
// {
//     for (auto const& f : api.functions)
//     {
//         if (f.second->api_info.name == name)
//             return *f.second;
//     }
//     throw undefined_function(name);
// }

// api_function_interface const&
// find_function_by_uid(api_implementation const& api, string const& uid)
// {
//     auto f = api
//     .functions.find(uid);
//     if (f == api.functions.end())
//         throw undefined_function(uid);
//     return *f->second;
// }

// api_implementation
// merge_apis(api_implementation const& a, api_implementation const& b)
// {
//     api_implementation merged;

//     for (auto const& i : a.types)
//         merged.types.push_back(i);
//     for (auto const& i : b.types)
//         merged.types.push_back(i);

//     for (auto const& i : a.functions)
//         register_api_function(merged, i.second);
//     for (auto const& i : b.functions)
//         register_api_function(merged, i.second);

//     for (auto const& i : a.upgrades)
//         merged.upgrades.push_back(i);
//     for (auto const& i : b.upgrades)
//         merged.upgrades.push_back(i);

//     for (auto const& i : a.records)
//         merged.records.push_back(i);
//     for (auto const& i : b.records)
//         merged.records.push_back(i);

//     return merged;
// }

// THE CRADLE API

// api_implementation
// get_cradle_api()
// {
//     api_implementation api;
//     CRADLE_REGISTER_APIS(api);
//     return api;
// }

// api_documentation
// get_api_documentation()
// {
//     return get_api_documentation(get_cradle_api());
// }

} // namespace cradle