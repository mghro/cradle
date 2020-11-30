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
  cpp_code_blocks
    [
      [
        "template<>";
        "struct definitive_type_info_query<" ^ e.enum_id ^ ">";
        "{";
        "    static void";
        "    get(cradle::api_type_info*);";
        "};";
      ];
      [
        "template<>";
        "struct type_info_query<" ^ e.enum_id ^ ">";
        "{";
        "    static void";
        "    get(cradle::api_type_info*);";
        "};";
      ];
      [
        "template<>";
        "struct enum_type_info_query<" ^ e.enum_id ^ ">";
        "{";
        "    static void";
        "    get(cradle::api_enum_info*);";
        "};";
      ];
    ]

let enum_type_info_definition app_id e =
  cpp_code_blocks
    [
      [
        "void";
        "definitive_type_info_query<" ^ e.enum_id ^ ">::get(";
        "    cradle::api_type_info* info)";
        "{";
        "    *info =";
        "        cradle::make_api_type_info_with_enum_type(";
        "            cradle::get_enum_type_info<" ^ e.enum_id ^ ">());";
        "}";
      ];
      [
        "void";
        "type_info_query<" ^ e.enum_id ^ ">::get(";
        "    cradle::api_type_info* info)";
        "{";
        "    *info =";
        "        cradle::make_api_type_info_with_named_type(";
        "            cradle::api_named_type_reference(";
        "                \"" ^ app_id ^ "\", \"" ^ e.enum_id ^ "\"));";
        "}";
      ];
      [
        "void";
        "enum_type_info_query<" ^ e.enum_id ^ ">::get(";
        "    cradle::api_enum_info* info)";
        "{";
        "    std::map<std::string, cradle::api_enum_value_info> values;";
        String.concat ""
          (List.map
             (fun v ->
               cpp_code_lines
                 [
                   "values[\"" ^ enum_value_id e v ^ "\"] = ";
                   "cradle::api_enum_value_info(";
                   "\"" ^ String.escaped v.ev_description ^ "\");";
                 ])
             e.enum_values);
        "    *info = cradle::api_enum_info(values);";
        "}";
      ];
    ]

(* Construct the options for the enum's upgrade function. *)
let construct_function_options app_id e =
  let make_function_parameter e =
    [
      {
        parameter_id = "v";
        parameter_type = [ Tid "cradle"; Tseparator; Tid "value" ];
        parameter_description = "value to upgrade";
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

(* Generate the registration code for the enum's upgrade function. *)
let enum_upgrade_register_function_instance account_id app_id e =
  if not (enum_is_internal e) then
    let f = construct_function_options app_id e in
    cpp_code_to_define_function_instance account_id app_id f "" []
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
    e.enum_id ^ " upgrade_value_" ^ e.enum_id ^ "(cradle::dynamic const& v);"
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
    e.enum_id ^ " upgrade_value_" ^ e.enum_id ^ "(cradle::dynamic const& v)"
    ^ "{" ^ e.enum_id ^ " x;" ^ "upgrade_value(&x, v); " ^ "return x; " ^ "}"
  else ""

let enum_query_declarations e =
  "static inline unsigned get_value_count(" ^ e.enum_id ^ ") " ^ "{ return "
  ^ string_of_int (List.length e.enum_values)
  ^ "; } " ^ "char const* get_value_id(" ^ e.enum_id ^ " value); "

let enum_query_definitions e =
  cpp_code_lines
    [
      "char const*";
      "get_value_id(" ^ e.enum_id ^ " value)";
      "{";
      "    switch (value)";
      "    {";
      String.concat ""
        (List.map
           (fun v ->
             cpp_code_lines
               [
                 "case " ^ e.enum_id ^ "::" ^ String.uppercase v.ev_id ^ ":";
                 "return \"" ^ enum_value_id e v ^ "\";";
               ])
           e.enum_values);
      "    }";
      "    CRADLE_THROW(";
      "        cradle::invalid_enum_value() <<";
      "            cradle::enum_id_info(\"" ^ e.enum_id ^ "\") <<";
      "            cradle::enum_value_info(int(value)));";
      "}";
    ]

let enum_hash_declaration namespace e =
  cpp_code_lines
    [
      "inline size_t";
      "hash_value(" ^ e.enum_id ^ " const& x)";
      "{";
      "    return size_t(x);";
      "}";
    ]

let enum_hash_definition namespace e = ""

let enum_conversion_declarations e =
  cpp_code_blocks
    [
      [
        "void";
        "to_dynamic(";
        "    cradle::dynamic* v,";
        "    " ^ e.enum_id ^ " x);";
      ];
      [
        "void";
        "from_dynamic(";
        "    " ^ e.enum_id ^ "* x,";
        "    cradle::dynamic const& v);";
      ];
      [
        "std::ostream&";
        "operator<<(";
        "    std::ostream& s,";
        "     " ^ e.enum_id ^ " const& x);";
      ];
    ]

let enum_conversion_definitions e =
  cpp_code_blocks
    [
      [
        "void";
        "to_dynamic(";
        "    cradle::dynamic* v,";
        "    " ^ e.enum_id ^ " x)";
        "{";
        "    *v = get_value_id(x);";
        "}";
      ];
      [
        "void";
        "from_dynamic(";
        "    " ^ e.enum_id ^ "* x,";
        "    cradle::dynamic const& v)";
        "{";
        "    string s = cast<string>(v);";
        String.concat ""
          (List.map
             (fun v ->
               cpp_indented_code_lines "    "
                 [
                   "if (boost::to_lower_copy(s) == \""
                   ^ String.lowercase v.ev_id ^ "\")";
                   "{";
                   "    *x = " ^ e.enum_id ^ "::" ^ String.uppercase v.ev_id
                   ^ ";";
                   "    return;";
                   "};";
                 ])
             e.enum_values);
        "    CRADLE_THROW(";
        "        cradle::invalid_enum_string() <<";
        "            cradle::enum_id_info(\"" ^ e.enum_id ^ "\") <<";
        "            cradle::enum_string_info(s));";
        "}";
      ];
      [
        "std::ostream&";
        "operator<<(std::ostream& s, " ^ e.enum_id ^ " const& x)";
        "{";
        "    s << get_value_id(x);";
        "    return s;";
        "}";
      ];
    ]

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

(* ^ enum_upgrade_type_declarations e
   ^ enum_upgrade_declaration e *)

let cpp_string_of_enum account_id app_id namespace e =
  enum_type_info_definition app_id e
  ^ enum_hash_definition namespace e
  ^ enum_query_definitions e
  ^ enum_conversion_definitions e

(* ^ enum_upgrade_type_definition e
   ^ enum_upgrade_definition e
   ^ enum_upgrade_register_function_instance account_id app_id e *)

(* Generate C++ code to register API function for upgrading values *)
let cpp_code_to_register_upgrade_function_instance e =
  let full_public_name = "upgrade_value_" ^ e.enum_id in
  "\nregister_api_function(api, " ^ "cradle::api_function_ptr(new "
  ^ full_public_name ^ "_fn_def)); "

let cpp_code_to_register_enum app_id e =
  if not (enum_is_internal e) then
    cpp_code_lines
      [
        "register_api_named_type(";
        "    api,";
        "    \"" ^ e.enum_id ^ "\",";
        "    " ^ string_of_int (get_enum_revision e) ^ ",";
        "    \"" ^ String.escaped e.enum_description ^ "\",";
        "    get_definitive_type_info<" ^ e.enum_id ^ ">());";
        (* "    get_upgrade_type(" ^ e.enum_id ^ "(), std::vector<std::type_index>())); " *)
      ]
  else ""

(* Generate the C++ code to clean up the #define namespace for an enum. *)
let cpp_cleanup_code_for_enum e =
  String.concat ""
    (List.map
       (fun v ->
         let uppercase_id = String.uppercase v.ev_id in
         cpp_code_lines
           [
             "\n#ifdef " ^ uppercase_id;
             "\n    #undef " ^ uppercase_id;
             "\n#endif";
             "\n";
           ])
       e.enum_values)
