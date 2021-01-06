#ifndef CRADLE_THINKNODE_TYPES_HPP
#define CRADLE_THINKNODE_TYPES_HPP

#include <cradle/common.hpp>

namespace cradle {

api(enum)
enum class thinknode_service_id
{
    IAM,
    APM,
    ISS,
    CALC,
    CAS,
    RKS,
    IMMUTABLE
};

api(struct)
struct thinknode_session
{
    std::string api_url;
    std::string access_token;
};

struct thinknode_array_info;
struct thinknode_blob_type;
struct thinknode_boolean_type;
struct thinknode_datetime_type;
struct thinknode_dynamic_type;
struct thinknode_enum_info;
struct thinknode_float_type;
struct thinknode_integer_type;
struct thinknode_map_info;
struct thinknode_named_type_reference;
struct thinknode_nil_type;
struct thinknode_type_info;
struct thinknode_string_type;
struct thinknode_structure_info;
struct thinknode_union_info;

api(union)
union thinknode_type_info
{
    thinknode_array_info array_type;
    thinknode_blob_type blob_type;
    thinknode_boolean_type boolean_type;
    thinknode_datetime_type datetime_type;
    thinknode_dynamic_type dynamic_type;
    thinknode_enum_info enum_type;
    thinknode_float_type float_type;
    thinknode_integer_type integer_type;
    thinknode_map_info map_type;
    thinknode_named_type_reference named_type;
    thinknode_nil_type nil_type;
    thinknode_type_info optional_type;
    thinknode_type_info reference_type;
    thinknode_string_type string_type;
    thinknode_structure_info structure_type;
    thinknode_union_info union_type;
};

struct function_application;
struct calculation_array_request;
struct calculation_item_request;
struct calculation_object_request;
struct calculation_property_request;
struct let_calculation_request;
struct meta_calculation_request;
struct calculation_cast_request;

api(union)
union calculation_request
{
    std::string reference;
    dynamic value;
    function_application function;
    calculation_array_request array;
    calculation_item_request item;
    calculation_object_request object;
    calculation_property_request property;
    let_calculation_request let;
    std::string variable;
    meta_calculation_request meta;
    calculation_cast_request cast;
};

api(struct)
struct function_application
{
    std::string account;
    std::string app;
    std::string name;
    boost::optional<cradle::integer> level;
    std::vector<cradle::calculation_request> args;
};

api(struct)
struct calculation_array_request
{
    std::vector<cradle::calculation_request> items;
    cradle::thinknode_type_info item_schema;
};

api(struct)
struct calculation_object_request
{
    std::map<std::string, cradle::calculation_request> properties;
    cradle::thinknode_type_info schema;
};

api(struct)
struct calculation_item_request
{
    cradle::calculation_request array;
    cradle::calculation_request index;
    cradle::thinknode_type_info schema;
};

api(struct)
struct calculation_property_request
{
    cradle::calculation_request object;
    cradle::calculation_request field;
    cradle::thinknode_type_info schema;
};

api(struct)
struct meta_calculation_request
{
    cradle::calculation_request generator;
    cradle::thinknode_type_info schema;
};

api(struct)
struct calculation_cast_request
{
    cradle::thinknode_type_info schema;
    cradle::calculation_request object;
};

api(struct)
struct let_calculation_request
{
    std::map<std::string, cradle::calculation_request> variables;
    cradle::calculation_request in;
};

// This describes an HTTP response with just an ID field.
api(struct)
struct id_response
{
    std::string id;
};

api(struct)
struct reported_calculation_info
{
    // the Thinknode ID of the calculation
    std::string id;
    // a label for the calculation - Currently, this is just the function name.
    std::string label;
};

api(struct)
struct let_calculation_submission_info
{
    // the ID of the top-level calculation
    std::string main_calc_id;
    // info on any subcalculations whose progress we're interested in
    std::vector<cradle::reported_calculation_info> reported_subcalcs;
    // IDs of any other subcalculations
    std::vector<std::string> other_subcalc_ids;
};

// This augments a normal Thinknode calculation request with extra information
// that's useful for status reporting.
api(struct)
struct augmented_calculation_request
{
    // the underlying request
    cradle::calculation_request request;
    // any variables that should be reported on
    std::vector<std::string> reported_variables;
};

api(enum)
enum class calculation_queue_type
{
    PENDING,
    READY
};

api(struct)
struct calculation_calculating_status
{
    double progress;
};

api(struct)
struct calculation_uploading_status
{
    double progress;
};

api(struct)
struct calculation_failure_status
{
    std::string code;
    std::string error;
    std::string message;
};

api(union)
union calculation_status
{
    calculation_calculating_status calculating;
    nil_t canceled;
    nil_t completed;
    calculation_failure_status failed;
    nil_t generating;
    calculation_queue_type queued;
    calculation_uploading_status uploading;
    nil_t waiting;
};

api(struct)
struct thinknode_nil_type
{
};

api(struct)
struct thinknode_boolean_type
{
};

api(struct)
struct thinknode_integer_type
{
};

api(struct)
struct thinknode_float_type
{
};

api(struct)
struct thinknode_string_type
{
};

api(struct)
struct thinknode_datetime_type
{
};

api(struct)
struct thinknode_blob_type
{
};

api(struct)
struct thinknode_dynamic_type
{
};

api(struct)
struct thinknode_structure_field_info
{
    std::string description;
    cradle::omissible<bool> omissible;
    cradle::thinknode_type_info schema;
};

api(struct)
struct thinknode_structure_info
{
    std::map<std::string, cradle::thinknode_structure_field_info> fields;
};

api(struct)
struct thinknode_union_member_info
{
    std::string description;
    cradle::thinknode_type_info schema;
};

api(struct)
struct thinknode_union_info
{
    std::map<std::string, cradle::thinknode_union_member_info> members;
};

api(struct)
struct thinknode_enum_value_info
{
    std::string description;
};

api(struct)
struct thinknode_enum_info
{
    std::map<std::string, cradle::thinknode_enum_value_info> values;
};

api(struct)
struct thinknode_array_info
{
    cradle::thinknode_type_info element_schema;
    // If size is absent, any size is acceptable.
    omissible<cradle::integer> size;
};

api(struct)
struct thinknode_map_info
{
    cradle::thinknode_type_info key_schema;
    cradle::thinknode_type_info value_schema;
};

api(struct)
struct thinknode_named_type_reference
{
    omissible<std::string> account;
    std::string app;
    std::string name;
};

api(struct)
struct thinknode_function_parameter_info
{
    std::string name;
    std::string description;
    cradle::thinknode_type_info schema;
};

api(struct)
struct thinknode_function_result_info
{
    std::string description;
    cradle::thinknode_type_info schema;
};

api(struct)
struct thinknode_function_type_info
{
    std::vector<cradle::thinknode_function_parameter_info> parameters;
    cradle::thinknode_function_result_info returns;
};

api(union)
union thinknode_function_type
{
    thinknode_function_type_info function_type;
};

api(struct)
struct thinknode_function_info
{
    std::string name;
    std::string description;
    std::string execution_class;
    cradle::thinknode_function_type schema;
};

api(struct)
struct thinknode_named_type_info
{
    std::string name;
    std::string description;
    cradle::thinknode_type_info schema;
};

api(union)
union thinknode_provider_image_info
{
    // a reference to a docker image by tag
    std::string tag;
    // a reference to a docker image by digest
    std::string digest;
};

api(struct)
struct thinknode_private_provider_info
{
    cradle::thinknode_provider_image_info image;
};

api(union)
union thinknode_provider_info
{
    thinknode_private_provider_info private;
};

api(struct)
struct thinknode_app_manifest
{
    // an array of objects that denote the app and version on which the
    // manifest depends
    std::vector<cradle::dynamic> dependencies;
    // a docker container providing function and upgrade implementations
    boost::optional<cradle::thinknode_provider_info> provider;
    // an array of types
    std::vector<cradle::thinknode_named_type_info> types;
    // an array of functions
    std::vector<cradle::thinknode_function_info> functions;
    // an array of records
    std::vector<cradle::dynamic> records;
    // an array of upgrade functions to use when migrating data from one
    // version to another
    std::vector<cradle::dynamic> upgrades;
};

api(struct)
struct thinknode_app_version_info
{
    // the name of the version
    std::string name;
    // the manifest for the version
    boost::optional<cradle::thinknode_app_manifest> manifest;
    // the user that created the version
    cradle::dynamic created_by;
    // the date and time when the version was created
    boost::posix_time::ptime created_at;
};

api(union)
union thinknode_app_source_info
{
    // the name of the source version
    std::string version;

    // the name of the source branch
    std::string branch;

    // the unique id of the source commit - Note that this is only returned if
    // the associated commit is not the current head of a branch
    std::string commit;
};

api(struct)
struct thinknode_context_app_info
{
    // the name of the account responsible for publishing the app
    std::string account;
    // the name of the app
    std::string app;
    cradle::thinknode_app_source_info source;
};

api(struct)
struct thinknode_context_contents
{
    // the bucket for the context
    std::string bucket;
    // a list of apps within the context
    std::vector<cradle::thinknode_context_app_info> contents;
};

api(struct)
struct thinknode_supervisor_calculation_request
{
    // the name of the function to invoke
    std::string name;
    // the arguments to the function
    std::vector<cradle::dynamic> args;
};

api(union)
union thinknode_supervisor_message
{
    thinknode_supervisor_calculation_request function;
    std::string ping;
};

api(struct)
struct thinknode_provider_progress_update
{
    double value;
    std::string message;
};

api(struct)
struct thinknode_provider_failure
{
    std::string code;
    std::string message;
};

api(struct)
struct thinknode_provider_registration
{
    // protocol version
    cradle::integer protocol;
    // unique ID assigned to this provider
    std::string pid;
};

api(union)
union thinknode_provider_message
{
    thinknode_provider_registration registration;
    thinknode_provider_progress_update progress;
    std::string pong;
    dynamic result;
    thinknode_provider_failure failure;
};

} // namespace cradle

#endif
