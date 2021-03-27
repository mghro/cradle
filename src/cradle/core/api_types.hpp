#ifndef CRADLE_API_TYPES_HPP
#define CRADLE_API_TYPES_HPP

#include <cradle/core/flags.hpp>
#include <cradle/core/monitoring.hpp>
#include <cradle/core/type_definitions.hpp>
#include <cradle/core/upgrades.hpp>

namespace cradle {

struct api_nil_type;
struct api_boolean_type;
struct api_integer_type;
struct api_float_type;
struct api_string_type;
struct api_datetime_type;
struct api_blob_type;
struct api_dynamic_type;
struct api_structure_info;
struct api_union_info;
struct api_enum_info;
struct api_array_info;
struct api_map_info;
struct api_named_type_reference;
struct api_function_type_info;
struct api_record_info;
struct api_structure_field_info;

api(union)
union api_type_info
{
    api_nil_type nil_type;
    api_boolean_type boolean_type;
    api_integer_type integer_type;
    api_float_type float_type;
    api_string_type string_type;
    api_datetime_type datetime_type;
    api_blob_type blob_type;
    api_dynamic_type dynamic_type;
    api_structure_info structure_type;
    api_union_info union_type;
    api_enum_info enum_type;
    api_map_info map_type;
    api_array_info array_type;
    api_type_info optional_type;
    api_type_info reference_type;
    api_named_type_reference named_type;
    api_function_type_info function_type;
    api_record_info record_type;
};

api(struct)
struct api_nil_type
{
};

api(struct)
struct api_boolean_type
{
};

api(struct)
struct api_integer_type
{
};

api(struct)
struct api_float_type
{
};

api(struct)
struct api_string_type
{
};

api(struct)
struct api_datetime_type
{
};

api(struct)
struct api_blob_type
{
};

api(struct)
struct api_dynamic_type
{
};

api(struct)
struct api_structure_field_info
{
    string description;
    api_type_info schema;
    cradle::omissible<bool> omissible;
};

api(struct)
struct api_structure_info
{
    std::map<string, api_structure_field_info> fields;
};

api(struct)
struct api_union_member_info
{
    string description;
    api_type_info schema;
};

api(struct)
struct api_union_info
{
    std::map<string, api_union_member_info> members;
};

api(struct)
struct api_enum_value_info
{
    string description;
};

api(struct)
struct api_enum_info
{
    std::map<string, api_enum_value_info> values;
};

api(struct)
struct api_array_info
{
    // If size is absent, any size is acceptable.
    omissible<integer> size;
    api_type_info element_schema;
};

api(struct)
struct api_map_info
{
    api_type_info key_schema, value_schema;
};

api(struct)
struct api_named_type_reference
{
    string app;
    string name;
};

api(struct)
struct api_record_type_info
{
    api_type_info schema;
};

api(struct)
struct api_function_parameter_info
{
    string name;
    api_type_info schema;
    string description;
};

api(struct)
struct api_function_result_info
{
    api_type_info schema;
    string description;
};

api(struct)
struct api_function_type_info
{
    std::vector<api_function_parameter_info> parameters;
    api_function_result_info returns;
};

api(struct)
struct api_function_upgrade_type_info
{
    string version, type, function;
};

api(struct)
struct api_function_info
{
    string name, description;

    string execution_class;

    api_type_info schema;
};

api(struct)
struct api_upgrade_function_info
{
    string version, type, function;
};

CRADLE_DEFINE_FLAG_TYPE(api_function)

struct api_function_implementation_info
{
    string account_id;
    string app_id;
    api_function_flag_set flags;
    string uid;
    string upgrade_version;
    int level;
};

// api_function_interface defines the run-time interface to a C++ function
// that's necessary to provide documentation and make it available for
// external invocation (e.g., over a network or via a scripting language).
struct api_function_interface
{
    virtual ~api_function_interface()
    {
    }

    api_function_info api_info;
    api_function_implementation_info implementation_info;

    virtual dynamic
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        dynamic_array const& args) const = 0;
    virtual dynamic
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        dynamic_map const& args) const = 0;
    virtual untyped_immutable
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        std::vector<untyped_immutable> const& args) const = 0;
};

// If this flag is set, the function will actually use the check in and
// progress reporting interfaces.
CRADLE_DEFINE_FLAG(api_function, 0x0001, FUNCTION_HAS_MONITORING)

static inline bool
has_monitor(api_function_interface const& f)
{
    return f.implementation_info.flags & FUNCTION_HAS_MONITORING;
}

// If this flag is set, the function is marked as trivial and won't be
// dispatched to a separate thread.
CRADLE_DEFINE_FLAG(api_function, 0x0002, FUNCTION_IS_TRIVIAL)

static inline bool
is_trivial(api_function_interface const& f)
{
    return f.implementation_info.flags & FUNCTION_IS_TRIVIAL;
}

// If this flag is set, the function is only available remotely.
CRADLE_DEFINE_FLAG(api_function, 0x0004, FUNCTION_IS_REMOTE)

static inline bool
is_remote(api_function_interface const& f)
{
    return f.implementation_info.flags & FUNCTION_IS_REMOTE;
}

// If this flag is set, results from the function are cached to disk.
CRADLE_DEFINE_FLAG(api_function, 0x0008, FUNCTION_IS_DISK_CACHED)

static inline bool
is_disk_cached(api_function_interface const& f)
{
    return f.implementation_info.flags & FUNCTION_IS_DISK_CACHED;
}

// If this flag is set, this function is an upgrade function.
CRADLE_DEFINE_FLAG(api_function, 0x0010, FUNCTION_IS_UPGRADE)

static inline bool
is_upgrade(api_function_implementation_info const& f)
{
    return (f.flags & FUNCTION_IS_UPGRADE) && f.upgrade_version != "0.0.0";
}

// If this flag is set, the progress of this function should be reported to
// the user.
CRADLE_DEFINE_FLAG(api_function, 0x0020, FUNCTION_IS_REPORTED)

static inline bool
is_reported(api_function_interface const& f)
{
    return f.implementation_info.flags & FUNCTION_IS_REPORTED;
}

typedef std::shared_ptr<api_function_interface> api_function_ptr;

api(struct)
struct api_named_type_info
{
    string name;
    string description;
    api_type_info schema;
};

api(struct)
struct api_named_type_implementation_info
{
    string name;
    string description;
    upgrade_type upgrade;
    api_type_info schema;
};

api(struct internal)
struct api_mutation_type_info
{
    string version;
    string type;
    string body;
};

api(union internal)
union upgrade_type_info
{
    dynamic mutation_type;
    api_upgrade_function_info upgrade_type;
};

api(struct internal)
struct api_upgrade_type_info
{
    string name;
    string description;
    upgrade_type_info schema;
};

api(struct internal)
struct api_dependency_type_info
{
    string account;
    string app;
    string version;
};

api(struct)
struct api_record_named_type_info
{
    string name;
    omissible<string> app;
    omissible<string> account;
};

api(struct)
struct api_record_named_type_schema
{
    api_record_named_type_info named_type;
};

api(struct)
struct api_record_info
{
    api_record_named_type_schema schema;
};

api(struct internal)
struct api_named_record_type_info
{
    string name;
    string description;
    api_type_info schema;
};

struct api_implementation
{
    std::vector<api_named_type_implementation_info> types;

    // functions are indexed by UID
    std::map<string, api_function_ptr> functions;

    std::vector<api_upgrade_type_info> upgrades;

    std::vector<api_named_record_type_info> records;
};

void
register_api_named_type(
    api_implementation& api,
    string const& name,
    unsigned version,
    string const& description,
    api_type_info const& form,
    upgrade_type upgrade = upgrade_type::NONE);

void
register_api_record_type(
    api_implementation& api,
    string const& record_name,
    string const& description,
    string const& account,
    string const& app,
    string const& name);

void
register_api_mutation_type(
    api_implementation& api,
    string const& description,
    string const& upgrade_version,
    string const& upgrade_type,
    string const& body);

std::vector<api_upgrade_type_info>
generate_api_upgrades(api_implementation const& api);

void
register_api_function(api_implementation& api, api_function_ptr const& f);

api(struct internal)
struct api_function_uid_contents
{
    string name;
    std::vector<api_function_parameter_info> parameters;
    unsigned revision;
};

// Given a function's name, its parameter info, and a revision number, this
// generates a UID for the function.
string
generate_function_uid(
    string const& name,
    std::vector<api_function_parameter_info> const& parameters,
    unsigned revision);

api(struct internal)
struct api_documentation
{
    std::vector<api_named_type_info> types;

    std::vector<api_function_info> functions;

    std::vector<api_upgrade_type_info> upgrades;

    std::vector<api_dependency_type_info> dependencies;

    std::vector<api_named_record_type_info> records;
};

api(struct internal)
struct api_manifest : api_documentation
{
    string var1;
};

api_documentation
get_api_documentation(
    api_implementation const& api, bool include_upgrade_functions = false);

string
get_api_implementation_documentation(api_implementation const& api);

string
get_manifest_json(api_implementation const& api);

// Returns only functions that are upgrade functions
string
get_manifest_json_with_upgrades(api_implementation const& api);

// Returns only functions that are upgrade functions
api_documentation
get_api_upgrade_documentation(api_implementation const& api);

// struct undefined_function : exception
// {
//     undefined_function(string const& name)
//         : exception("undefined function: " + name), name_(new string(name))
//     {
//     }
//     string const&
//     name()
//     {
//         return *name_;
//     }
//     ~undefined_function() throw()
//     {
//     }

//  private:
//     std::shared_ptr<string> name_;
// };

api_function_interface const&
find_function_by_name(api_implementation const& api, string const& name);

api_function_interface const&
find_function_by_uid(api_implementation const& api, string const& uid);

// struct undefined_type : exception
// {
//     undefined_type(string const& name)
//         : exception("undefined type: " + name), name_(new string(name))
//     {
//     }
//     string const&
//     name()
//     {
//         return *name_;
//     }
//     ~undefined_type() throw()
//     {
//     }

//  private:
//     std::shared_ptr<string> name_;
// };

// // Merge two APIs together.
// //
// // The resulting API will contain all the types, records, functions, and
// // upgrades from both apps. It does *not* try to reconcile the two lists, so
// // there should be no overlap between the two.
// //
// api_implementation
// merge_apis(api_implementation const& a, api_implementation const& b);

// // THE CRADLE API

// api_implementation
// get_cradle_api();

// api_documentation
// get_api_documentation();

} // namespace cradle

#endif
