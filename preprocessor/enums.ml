(* This file contains code for preprocessing C++ enums. *)

open Types
open Utilities
open Functions

let enum_preserves_case e =
  List.exists
    (fun o -> match o with EOpreserve_case -> true | _ -> false)
    e.enum_options

let enum_is_internal e =
  List.exists
    (fun o -> match o with EOinternal -> true | _ -> false)
    e.enum_options

let get_enum_revision e =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | EOrevision _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec get_revision_option options =
    match options with
    | [] -> 0
    | o :: rest -> (
        match o with
        | EOrevision v ->
            check_for_duplicates rest;
            v
        | _ -> get_revision_option rest )
  in
  get_revision_option e.enum_options

let enum_value_id e v =
  if enum_preserves_case e then v.ev_id else String.lowercase v.ev_id

let enum_declaration e =
  "enum class " ^ e.enum_id ^ " " ^ "{ "
  ^ String.concat ","
      (List.map (fun v -> String.uppercase v.ev_id) e.enum_values)
  ^ " " ^ "}; "

let enum_type_info_declaration e =
  "cradle::raw_type_info get_proper_type_info(" ^ e.enum_id ^ "); "
  ^ "cradle::raw_type_info get_type_info(" ^ e.enum_id ^ "); "
  ^ "cradle::raw_enum_info get_enum_type_info(" ^ e.enum_id ^ "); "

let enum_type_info_definition app_id e =
  "cradle::raw_enum_info get_enum_type_info(" ^ e.enum_id ^ ") " ^ "{ "
  ^ "std::vector<cradle::raw_enum_value_info> values; "
  ^ String.concat ""
      (List.map
         (fun v ->
           "values.push_back(cradle::raw_enum_value_info(" ^ "\""
           ^ enum_value_id e v ^ "\"," ^ "\""
           ^ String.escaped v.ev_description
           ^ "\")); ")
         e.enum_values)
  ^ "return cradle::raw_enum_info( " ^ "\"" ^ e.enum_id ^ "\"," ^ "\""
  ^ String.escaped e.enum_description
  ^ "\"," ^ "values); " ^ "} " ^ "cradle::raw_type_info get_proper_type_info("
  ^ e.enum_id ^ ") " ^ "{ " ^ "return cradle::raw_type_info(raw_kind::ENUM, "
  ^ "any(get_enum_type_info(" ^ e.enum_id ^ "()))); " ^ "} "
  ^ "cradle::raw_type_info get_type_info(" ^ e.enum_id ^ ") " ^ "{ "
  ^ "return cradle::raw_type_info(raw_kind::NAMED_TYPE_REFERENCE, "
  ^ "any(raw_named_type_reference(\"" ^ app_id ^ "\", \"" ^ e.enum_id
  ^ "\"))); " ^ "} "

(* Generate the function definition for API function that is generated to upgrade the enum. *)
let construct_function_options app_id e =
  let make_function_parameter e =
    [
      {
        parameter_id = "v";
        parameter_type = [ Tid "cradle"; Tseparator; Tid "value" ];
        parameter_description = "value to update";
        parameter_by_reference = true;
      };
    ]
  in
  let make_return_type e = [ Tid e.enum_id ] in

  {
    function_variants = [];
    function_id = "upgrade_value_" ^ e.enum_id;
    function_description = "upgrade function for " ^ e.enum_id;
    function_template_parameters = [];
    function_parameters = make_function_parameter e;
    function_return_type = make_return_type e;
    function_return_description = "upgraded value for " ^ e.enum_id;
    function_body = None;
    function_has_monitoring = false;
    function_is_trivial = false;
    function_is_remote = true;
    function_is_internal = false;
    function_is_disk_cached = false;
    function_is_reported = false;
    function_revision = 0;
    function_public_name = "upgrade_value_" ^ e.enum_id;
    function_execution_class = "cpu.x1";
    function_upgrade_version = "0.0.0";
    function_level = 1;
  }

(* Makes call to register API function for upgrading value for enum. *)
let enum_upgrade_register_function_instance account_id app_id e =
  if not (enum_is_internal e) then
    let f = construct_function_options app_id e in
    cpp_code_to_define_function_instance account_id app_id f "" []
    (* f.function_parameters     *)
  else ""

(* Generate the declaration for getting the upgrade type for the enum. *)
let enum_upgrade_type_declarations e =
  if not (enum_is_internal e) then
    "cradle::upgrade_type get_upgrade_type(" ^ e.enum_id
    ^ " const&, std::vector<std::type_index> parsed_types); "
  else ""

(* Generate the declaration for upgrading the enum. *)
let enum_upgrade_declaration e =
  if not (enum_is_internal e) then
    e.enum_id ^ " upgrade_value_" ^ e.enum_id ^ "(cradle::value const& v);"
  else ""

(* Generate the definition for getting the upgrade type for the enum. *)
let enum_upgrade_type_definition e =
  if not (enum_is_internal e) then
    "cradle::upgrade_type get_upgrade_type(" ^ e.enum_id
    ^ " const&, std::vector<std::type_index> parsed_types)" ^ "{ "
    ^ "using cradle::get_explicit_upgrade_type;"
    ^ "cradle::upgrade_type type = get_explicit_upgrade_type(" ^ e.enum_id
    ^ "()); " ^ "return type;" ^ "} "
  else ""

(* Generate the definition for upgrading the enum. *)
let enum_upgrade_definition e =
  if not (enum_is_internal e) then
    e.enum_id ^ " upgrade_value_" ^ e.enum_id ^ "(cradle::value const& v)" ^ "{"
    ^ e.enum_id ^ " x;" ^ "upgrade_value(&x, v); " ^ "return x; " ^ "}"
  else ""

let enum_query_declarations e =
  "static inline unsigned get_value_count(" ^ e.enum_id ^ ") " ^ "{ return "
  ^ string_of_int (List.length e.enum_values)
  ^ "; } " ^ "char const* get_value_id(" ^ e.enum_id ^ " value); "

let enum_query_definitions e =
  "char const* get_value_id(" ^ e.enum_id ^ " value) " ^ "{ "
  ^ "switch (value) " ^ "{ "
  ^ String.concat ""
      (List.map
         (fun v ->
           "case " ^ e.enum_id ^ "::"
           ^ String.uppercase v.ev_id
           ^ ": " ^ "return \"" ^ enum_value_id e v ^ "\"; ")
         e.enum_values)
  ^ "} " ^ "throw invalid_enum_value(get_enum_type_info(value), "
  ^ "unsigned(value)); " ^ "} "

let enum_hash_declaration namespace e =
  "} namespace std { " ^ "template<> " ^ "struct hash<" ^ namespace ^ "::"
  ^ e.enum_id ^ " > " ^ "{ " ^ "size_t operator()(" ^ namespace ^ "::"
  ^ e.enum_id ^ " x) const " ^ "{ return hash<int>()(static_cast<int>(x)); } "
  ^ "}; " ^ "} namespace " ^ namespace ^ " { "

let enum_hash_definition namespace e = ""

let enum_conversion_declarations e =
  "void to_value(cradle::value* v, " ^ e.enum_id ^ " x); " ^ "void from_value("
  ^ e.enum_id ^ "* x, cradle::value const& v); "
  ^ "std::ostream& operator<<(std::ostream& s, " ^ e.enum_id ^ " const& x); "

let enum_conversion_definitions e =
  "void to_value(cradle::value* v, " ^ e.enum_id ^ " x) " ^ "{ "
  ^ "set(*v, get_value_id(x)); " ^ "} " ^ "void from_value(" ^ e.enum_id
  ^ "* x, cradle::value const& v) " ^ "{ " ^ "string s = cast<string>(v); "
  ^ String.concat ""
      (List.map
         (fun v ->
           "if (boost::to_lower_copy(s) == \""
           ^ String.lowercase v.ev_id
           ^ "\") " ^ "{ " ^ "*x = " ^ e.enum_id ^ "::"
           ^ String.uppercase v.ev_id
           ^ "; " ^ "return; " ^ "} ")
         e.enum_values)
  ^ "throw invalid_enum_string(get_enum_type_info(" ^ e.enum_id ^ "()), s); "
  ^ "}" ^ "std::ostream& operator<<(std::ostream& s, " ^ e.enum_id
  ^ " const& x) " ^ "{ s << get_value_id(x); return s; } "

let enum_deep_sizeof_definition e =
  "static inline size_t deep_sizeof(" ^ e.enum_id ^ ") " ^ "{ return sizeof("
  ^ e.enum_id ^ "); } "

let hpp_string_of_enum account_id app_id namespace e =
  enum_declaration e
  ^ enum_type_info_declaration e
  ^ enum_deep_sizeof_definition e
  ^ enum_hash_declaration namespace e
  ^ enum_query_declarations e
  ^ enum_conversion_declarations e
  ^ enum_upgrade_type_declarations e
  ^ enum_upgrade_declaration e

let cpp_string_of_enum account_id app_id namespace e =
  enum_type_info_definition app_id e
  ^ enum_hash_definition namespace e
  ^ enum_query_definitions e
  ^ enum_conversion_definitions e
  ^ enum_upgrade_type_definition e
  ^ enum_upgrade_definition e
  ^ enum_upgrade_register_function_instance account_id app_id e

(* Generate C++ code to register API function for upgrading values *)
let cpp_code_to_register_upgrade_function_instance e =
  let full_public_name = "upgrade_value_" ^ e.enum_id in
  "\nregister_api_function(api, " ^ "cradle::api_function_ptr(new "
  ^ full_public_name ^ "_fn_def)); "

let cpp_code_to_register_enum app_id e =
  if not (enum_is_internal e) then
    "\nregister_api_named_type(api, " ^ "\"" ^ e.enum_id ^ "\", "
    ^ string_of_int (get_enum_revision e)
    ^ ", " ^ "\""
    ^ String.escaped e.enum_description
    ^ "\", " ^ "make_api_type_info(get_proper_type_info(" ^ e.enum_id ^ "())), "
    ^ "get_upgrade_type(" ^ e.enum_id
    ^ "(), std::vector<std::type_index>())); \n"
    ^ cpp_code_to_register_upgrade_function_instance e
  else ""
