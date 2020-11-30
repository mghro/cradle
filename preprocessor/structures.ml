(* This file contains code related to preprocessing C++ structures. *)

open Types
open Utilities
open Functions

(* Get the list of variants for a structure.
   Variants are specified as an optional parameter. *)
let get_structure_variants s =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | SOvariants _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec get_variants_option options =
    match options with
    | [] -> []
    | o :: rest -> (
        match o with
        | SOvariants v ->
            check_for_duplicates rest;
            v
        | _ -> get_variants_option rest )
  in
  get_variants_option s.structure_options

(* Given a list of preprocessor declarations, find the structure declaration
   with the given ID. If it's not found, raise an exception. *)
exception MissingStructure of string

let rec find_structure declarations id =
  match declarations with
  | [] -> raise (MissingStructure id)
  | t :: rest -> (
      match t with
      | Dstructure s ->
          if s.structure_id = id then s else find_structure rest id
      | _ -> find_structure rest id )

(* Generate the C++ code to declare a structure field. *)
let field_declaration f =
  cpp_code_for_type f.field_type ^ " " ^ f.field_id ^ ";"

(* Does s have template parameters? *)
let has_parameters s = List.length s.structure_parameters != 0

(* Get the list of preexisting components for a structure. *)
let get_structure_preexisting_components s =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | SOpreexisting _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec get_preexisting_components options =
    match options with
    | [] -> None
    | o :: rest -> (
        match o with
        | SOpreexisting p ->
            check_for_duplicates rest;
            Some p
        | _ -> get_preexisting_components rest )
  in
  get_preexisting_components s.structure_options

(* Is this a preexisting structure? *)
let structure_is_preexisting s =
  match get_structure_preexisting_components s with None -> false | _ -> true

(* Check if a particular component is flagged as preexisting. *)
let structure_component_is_preexisting s component =
  match get_structure_preexisting_components s with
  | None -> false
  | Some components -> List.mem component components

(* Is this an internal structure? *)
let structure_is_internal s =
  List.exists
    (fun o -> match o with SOinternal -> true | _ -> false)
    s.structure_options

let structure_is_record s =
  List.exists
    (fun o -> match o with SOrecord r -> true | _ -> false)
    s.structure_options

let structure_has_super s =
  match s.structure_super with Some super -> true | None -> false

let get_structure_record_name s =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | SOrecord _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec structure_record_name options =
    match options with
    | [] -> ""
    | o :: rest -> (
        match o with
        | SOrecord r ->
            check_for_duplicates rest;
            r
        | _ -> structure_record_name rest )
  in
  structure_record_name s.structure_options

(* Get the (optional) namespace override specified for a structure. *)
let get_structure_namespace_override s =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | SOnamespace _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec get_namespace_option options =
    match options with
    | [] -> None
    | o :: rest -> (
        match o with
        | SOnamespace n ->
            check_for_duplicates rest;
            Some n
        | _ -> get_namespace_option rest )
  in
  get_namespace_option s.structure_options

(* Resolve the namespace for a structure. *)
let resolve_structure_namespace app_namespace s =
  match get_structure_namespace_override s with
  | None -> app_namespace
  | Some n -> n

(* Get the revision number of the structure s. *)
let get_structure_revision s =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | SOrevision _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec get_revision_option options =
    match options with
    | [] -> 0
    | o :: rest -> (
        match o with
        | SOrevision v ->
            check_for_duplicates rest;
            v
        | _ -> get_revision_option rest )
  in
  get_revision_option s.structure_options

(* Generate the C++ code for the full type of a structure. *)
let full_structure_type s =
  if has_parameters s then
    s.structure_id ^ "<"
    ^ String.concat "," (List.map (fun (t, p) -> p) s.structure_parameters)
    ^ ">"
  else s.structure_id

(* Generate the C++ get_type_info function for a single instantiation of a
   structure. *)
let structure_type_info_definition_instance app_id label assignments s =
  let full_structure_name =
    s.structure_id
    ^ resolved_template_parameter_list assignments s.structure_parameters
  in

  let api_structure_name = s.structure_id ^ label in

  cpp_code_blocks
    [
      [
        "void";
        "definitive_type_info_query<" ^ full_structure_name ^ ">::get(";
        "    cradle::api_type_info* info)";
        "{";
        "    std::map<std::string, cradle::api_structure_field_info> fields;";
        "    structure_field_type_info_adder<" ^ full_structure_name
        ^ ">::add(&fields);";
        "    *info =";
        "        cradle::make_api_type_info_with_structure_type(";
        "            cradle::api_structure_info(";
        "                fields));";
        "}";
      ];
      [
        "void";
        "type_info_query<" ^ full_structure_name ^ ">::get(";
        "    cradle::api_type_info* info)";
        "{";
        "    *info =";
        "        cradle::make_api_type_info_with_named_type(";
        "            cradle::api_named_type_reference(";
        "                \"" ^ app_id ^ "\", \"" ^ api_structure_name ^ "\"));";
        "}";
      ];
      [
        "void";
        "structure_field_type_info_adder<" ^ full_structure_name ^ ">::add(";
        "    std::map<std::string, cradle::api_structure_field_info>* fields)";
        "{";
        ( match s.structure_super with
        | Some super ->
            "    structure_field_type_info_adder<" ^ super ^ ">::add(fields); "
        | None -> "" );
        String.concat ""
          (List.map
             (fun f ->
               cpp_indented_code_lines "    "
                 [
                   "(*fields)[\"" ^ f.field_id ^ "\"] =";
                   "    cradle::api_structure_field_info(";
                   "        \"" ^ String.escaped f.field_description ^ "\",";
                   "        cradle::get_type_info<decltype(std::declval<"
                   ^ full_structure_name ^ ">()." ^ f.field_id ^ ")>(),";
                   "        none);";
                 ])
             s.structure_fields);
        "}";
      ];
    ]

(* Generate the C++ code to determine the upgrade type. *)
let structure_upgrade_type_definition_instance app_id label assignments s =
  if not (structure_is_internal s) then
    "cradle::upgrade_type get_upgrade_type(" ^ s.structure_id
    ^ resolved_template_parameter_list assignments s.structure_parameters
    ^ " const&, std::vector<std::type_index> parsed_types)" ^ "{ "
    ^ "using cradle::get_explicit_upgrade_type;"
    ^ "using cradle::get_upgrade_type;"
    ^ "cradle::upgrade_type type = get_explicit_upgrade_type(" ^ s.structure_id
    ^ resolved_template_parameter_list assignments s.structure_parameters
    ^ "()); "
    ^
    if List.length s.structure_fields > 0 then
      String.concat ""
        (List.map
           (fun f ->
             "if(std::find(parsed_types.begin(), parsed_types.end(), \
              std::type_index(typeid("
             ^ cpp_code_for_parameterized_type assignments f.field_type
             ^ "()))) == parsed_types.end()) { "
             ^ "parsed_types.push_back(std::type_index(typeid("
             ^ cpp_code_for_parameterized_type assignments f.field_type
             ^ "()))); "
             ^ "type = cradle::merged_upgrade_type(type, get_upgrade_type("
             ^ cpp_code_for_parameterized_type assignments f.field_type
             ^ "(), parsed_types)); } ")
           s.structure_fields)
      ^ "return type; }"
    else "return type; }"
  else ""

(* Generate the C++ code for API function that will be used to upgrade the value. *)
let structure_upgrade_value_definition_api_instance app_id label assignments s =
  s.structure_id
  ^ resolved_template_parameter_list assignments s.structure_parameters
  ^ " upgrade_value_" ^ s.structure_id ^ label ^ "(cradle::dynamic const& v)"
  ^ "{" ^ s.structure_id
  ^ resolved_template_parameter_list assignments s.structure_parameters
  ^ " x;" ^ "upgrade_value(&x, v); " ^ "return x; " ^ "}"

(* If structure has no fields then the value_map of fields should not be created becuase
    it will be empty and unused causing compiler warnings. *)
let structure_upgrade_value_definition_instance_no_fields app_id label
    assignments s =
  "void auto_upgrade_value(" ^ s.structure_id
  ^ resolved_template_parameter_list assignments s.structure_parameters
  ^ " *x, cradle::dynamic const& v)" ^ "{ from_dynamic(x, v); } "
  ^ structure_upgrade_value_definition_api_instance app_id label assignments s

(* If structure has fields loop over them and update values for those fields. *)
let structure_upgrade_value_definition_instance_with_fields app_id label
    assignments s =
  "void auto_upgrade_value(" ^ s.structure_id
  ^ resolved_template_parameter_list assignments s.structure_parameters
  ^ " *x, cradle::dynamic const& v)"
  ^ "{  auto const& fields = cradle::cast<cradle::dynamic_map>(v); "
  ^ String.concat ""
      (List.map
         (fun f ->
           "cradle::upgrade_field(" ^ "&x->" ^ f.field_id ^ ", " ^ "fields, "
           ^ "\"" ^ f.field_id ^ "\");")
         s.structure_fields)
  ^ "} "
  ^ structure_upgrade_value_definition_api_instance app_id label assignments s

(* Check if structure has fields, and call appropriate function. *)
let structure_upgrade_value_definition_instance app_id label assignments s =
  match s.structure_fields with
  | [] ->
      structure_upgrade_value_definition_instance_no_fields app_id label
        assignments s
  | _ ->
      structure_upgrade_value_definition_instance_with_fields app_id label
        assignments s

(* Generate the function definition for API function that is generated to upgrade the structure. *)
let construct_function_options app_id label assignments s =
  let make_function_parameter s =
    [
      {
        parameter_id = "v";
        parameter_type = [ Tid "cradle"; Tseparator; Tid "value" ];
        parameter_description = "value to upgrade";
        parameter_by_reference = true;
      };
    ]
  in
  let make_return_type s assignments =
    [
      Tid
        ( s.structure_id
        ^ resolved_template_parameter_list assignments s.structure_parameters );
    ]
  in

  {
    function_variants = [];
    function_id = "upgrade_value_" ^ s.structure_id ^ label;
    function_description = "upgrade struct function for " ^ s.structure_id;
    function_template_parameters = [];
    function_parameters = make_function_parameter s;
    function_return_type = make_return_type s assignments;
    function_return_description = "upgraded struct value for " ^ s.structure_id;
    function_body = None;
    function_has_monitoring = false;
    function_is_trivial = false;
    function_is_remote = true;
    function_is_internal = false;
    function_is_disk_cached = false;
    function_is_reported = false;
    function_revision = 0;
    function_public_name = "upgrade_value_" ^ s.structure_id;
    function_execution_class = "cpu.x1";
    function_upgrade_version = "0.0.0";
    function_level = 1;
  }

(* Make call to register API function for upgrading value for structure. *)
let structure_upgrade_register_function_instance account_id app_id label
    assignments s =
  let f = construct_function_options app_id label assignments s in
  cpp_code_to_define_function_instance account_id app_id f label assignments

(* Generate API function for upgrading structure. *)
let structure_upgrade_register_function account_id app_id s =
  if not (structure_is_internal s) then
    let instantiations = enumerate_combinations (get_structure_variants s) in
    String.concat ""
      (List.map
         (fun (assignments, label) ->
           structure_upgrade_register_function_instance account_id app_id label
             assignments s)
         instantiations)
  else ""

(* Generate API function for determining the upgrade type. *)
let structure_upgrade_type_definition app_id s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_upgrade_type_definition_instance app_id label assignments s)
       instantiations)

(* Generate the declaration for upgrading the structure. *)
let structure_upgrade_value_definition app_id s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_upgrade_value_definition_instance app_id label assignments s)
       instantiations)

(* Generate the C++ get_type_info function for all instantiations of a
   structure. *)
let structure_type_info_definition app_id s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_type_info_definition_instance app_id label assignments s)
       instantiations)

(* Generate the C++ type_info_query declarations for a single instantiation of
   a structure. *)
let structure_type_info_declaration_instance label assignments s =
  let full_structure_name =
    s.structure_id
    ^ resolved_template_parameter_list assignments s.structure_parameters
  in
  cpp_code_blocks
    [
      [
        "template<>";
        "struct definitive_type_info_query<" ^ full_structure_name ^ ">";
        "{";
        "    static void";
        "    get(cradle::api_type_info*);";
        "};";
      ];
      [
        "template<>";
        "struct type_info_query<" ^ full_structure_name ^ ">";
        "{";
        "    static void";
        "    get(cradle::api_type_info*);";
        "};";
      ];
      [
        "template<>";
        "struct structure_field_type_info_adder<" ^ full_structure_name ^ ">";
        "{";
        "    static void";
        "    add(std::map<std::string, cradle::api_structure_field_info>*);";
        "};";
      ];
    ]

(* Generate the declaration for getting the upgrade type of a single
   instantation of the structure. *)
let structure_upgrade_type_declaration_instance label assignments s =
  if not (structure_is_internal s) then
    "cradle::upgrade_type get_upgrade_type(" ^ s.structure_id
    ^ resolved_template_parameter_list assignments s.structure_parameters
    ^ " const&, std::vector<std::type_index> parsed_types);"
  else ""

(* Generate the declaration for the function that will upgrade a single
   instantiation of the structure. *)
let structure_upgrade_value_declaration_instance label assignments s =
  "void auto_upgrade_value(" ^ s.structure_id
  ^ resolved_template_parameter_list assignments s.structure_parameters
  ^ " *x, cradle::dynamic const& v);"

(* Generate the declaration for getting the upgrade type. *)
let structure_upgrade_type_declaration s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_upgrade_type_declaration_instance label assignments s)
       instantiations)

(* Generate the declaration for the function that will upgrade structure. *)
let structure_upgrade_value_declaration s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_upgrade_value_declaration_instance label assignments s)
       instantiations)

(* Generate the C++ get_type_info declaration for all instantiations of a
   structure. *)
let structure_type_info_declaration s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_type_info_declaration_instance label assignments s)
       instantiations)

(* Generate the request constructor definition for one instance of a
   structure. *)
let structure_request_definition_instance label assignments s =
  match s.structure_super with
  | Some super ->
      (* This case is very hard because we don't know what fields are in
         the super structure, so it's not implemented yet. *)
      ""
  | None ->
      "cradle::request<" ^ s.structure_id
      ^ resolved_template_parameter_list assignments s.structure_parameters
      ^ "> rq_construct_" ^ s.structure_id ^ label ^ "("
      ^ String.concat ","
          (List.map
             (fun f ->
               "cradle::request<"
               ^ cpp_code_for_parameterized_type assignments f.field_type
               ^ "> const& " ^ f.field_id)
             s.structure_fields)
      ^ ") " ^ "{ "
      ^ "std::map<std::string,cradle::untyped_request> structure_fields_; "
      ^ String.concat ""
          (List.map
             (fun f ->
               "structure_fields_[\"" ^ f.field_id ^ "\"] = " ^ f.field_id
               ^ ".untyped; ")
             s.structure_fields)
      ^ "return cradle::rq_structure<" ^ s.structure_id
      ^ resolved_template_parameter_list assignments s.structure_parameters
      ^ ">(structure_fields_); " ^ "} "

(* Generate the request constructor definition for all instances of a
   structure. *)
let structure_request_definition s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_request_definition_instance label assignments s)
       instantiations)

(* Generate the request constructor declaration for one instance of a
   structure. *)
let structure_request_declaration_instance label assignments s =
  match s.structure_super with
  | Some super ->
      (* This case is very hard because we don't know what fields are in
         the super structure, so it's not implemented yet. *)
      ""
  | None ->
      "cradle::request<" ^ s.structure_id
      ^ resolved_template_parameter_list assignments s.structure_parameters
      ^ "> rq_construct_" ^ s.structure_id ^ label ^ "("
      ^ String.concat ","
          (List.map
             (fun f ->
               "cradle::request<"
               ^ cpp_code_for_parameterized_type assignments f.field_type
               ^ "> const& " ^ f.field_id)
             s.structure_fields)
      ^ "); "

(* Generate the request constructor declaration for all instances of a
   structure. *)
let structure_request_declaration s =
  let instantiations = enumerate_combinations (get_structure_variants s) in
  String.concat ""
    (List.map
       (fun (assignments, label) ->
         structure_request_declaration_instance label assignments s)
       instantiations)

(* Generate the C++ code to convert a structure to and from a dynamic value. *)
let structure_value_conversion_implementation s =
  template_parameters_declaration s.structure_parameters
  ^ "void write_fields_to_record(cradle::dynamic_map& record, "
  ^ full_structure_type s ^ " const& x) " ^ "{ "
  ^ "using cradle::write_field_to_record; "
  ^ ( match s.structure_super with
    | Some super -> "write_fields_to_record(record, as_" ^ super ^ "(x)); "
    | None -> "" )
  ^ String.concat ""
      (List.map
         (fun f ->
           "write_field_to_record(record, \"" ^ f.field_id ^ "\", x."
           ^ f.field_id ^ "); ")
         s.structure_fields)
  ^ "} "
  ^ template_parameters_declaration s.structure_parameters
  ^ "void to_dynamic(cradle::dynamic* v, " ^ full_structure_type s
  ^ " const& x) " ^ "{ " ^ "cradle::dynamic_map r; "
  ^ "write_fields_to_record(r, x); " ^ "*v = std::move(r); " ^ "} "
  ^ template_parameters_declaration s.structure_parameters
  ^ "void read_fields_from_record(" ^ full_structure_type s
  ^ "& x, cradle::dynamic_map const& record) " ^ "{ "
  ^ "using cradle::read_field_from_record; "
  ^ ( match s.structure_super with
    | Some super -> "read_fields_from_record(as_" ^ super ^ "(x), record); "
    | None -> "" )
  ^ String.concat ""
      (List.map
         (fun f ->
           "read_field_from_record(&x." ^ f.field_id ^ ", record, \""
           ^ f.field_id ^ "\"); ")
         s.structure_fields)
  ^ "} "
  ^ template_parameters_declaration s.structure_parameters
  ^ "void from_dynamic(" ^ full_structure_type s ^ "* x,"
  ^ " cradle::dynamic const& v) " ^ "{ " ^ "cradle::dynamic_map const& r = "
  ^ "cradle::cast<cradle::dynamic_map>(v); "
  ^ "read_fields_from_record(*x, r); " ^ "} "

(* ^ template_parameters_declaration s.structure_parameters
   ^ "void read_fields_from_immutable_map(" ^ full_structure_type s ^ "& x, "
   ^ "std::map<std::string,cradle::untyped_immutable> const& fields) " ^ "{ "
   ^ ( match s.structure_super with
     | Some super ->
         "read_fields_from_immutable_map(as_" ^ super ^ "(x), fields); "
     | None -> "" )
   ^ String.concat ""
       (List.map
          (fun f ->
            "try { " ^ "from_immutable(&x." ^ f.field_id ^ ", "
            ^ "cradle::get_field(fields, \"" ^ f.field_id ^ "\")); "
            ^ "} catch (cradle::exception& e) { " ^ "e.add_context(\"in field "
            ^ f.field_id ^ "\"); " ^ "throw; } ")
          s.structure_fields)
   ^ "} " *)

(* Generate the definitions of the conversion functions. *)
let structure_value_conversion_definitions s =
  if not (has_parameters s) then structure_value_conversion_implementation s
  else ""

(* Generate the declarations of the conversion functions. *)
let structure_value_conversion_declarations s =
  if not (has_parameters s) then
    "void write_fields_to_record(cradle::dynamic_map& record, " ^ s.structure_id
    ^ " const& x); " ^ "void to_dynamic(cradle::dynamic* v, " ^ s.structure_id
    ^ " const& x); " ^ "void read_fields_from_record(" ^ s.structure_id
    ^ "& x, cradle::dynamic_map const& record); " ^ "void from_dynamic("
    ^ s.structure_id ^ "* x," ^ " cradle::dynamic const& v); "
    (* ^ "void read_fields_from_immutable_map(" ^ full_structure_type s ^ "& x, "
       ^ "std::map<std::string,cradle::untyped_immutable> const& fields); " *)
    ^ "std::ostream& operator<<(std::ostream& s, "
    ^ s.structure_id ^ " const& x);"
  else structure_value_conversion_implementation s

(* Generate the iostream interface for a structure. *)
let structure_iostream_implementation s =
  template_parameters_declaration s.structure_parameters
  ^ "std::ostream& operator<<(std::ostream& s, " ^ full_structure_type s
  ^ " const& x) " ^ "{ return s << to_dynamic(x); } "

(* Generate the definitions of the stream functions. *)
let structure_iostream_definitions s =
  if not (has_parameters s) then structure_iostream_implementation s else ""

(* Generate the declarations of the stream functions. *)
let structure_iostream_declarations s =
  if not (has_parameters s) then
    "std::ostream& operator<<(std::ostream& s, " ^ s.structure_id
    ^ " const& x);"
  else structure_iostream_implementation s

(* Generate the declarations of the C++ comparison operators. *)
let structure_comparison_declarations s =
  if not (has_parameters s) then
    "bool operator==(" ^ s.structure_id ^ " const& a, " ^ s.structure_id
    ^ " const& b); " ^ "bool operator!=(" ^ s.structure_id ^ " const& a, "
    ^ s.structure_id ^ " const& b); " ^ "bool operator<(" ^ s.structure_id
    ^ " const& a, " ^ s.structure_id ^ " const& b); "
  else
    template_parameters_declaration s.structure_parameters
    ^ "bool operator==(" ^ full_structure_type s ^ " const& a, "
    ^ full_structure_type s ^ " const& b) " ^ "{ " ^ "return "
    ^ ( if List.length s.structure_fields > 0 then
        ( match s.structure_super with
        | Some super -> "as_" ^ super ^ "(a) == as_" ^ super ^ "(b) && "
        | None -> "" )
        ^ String.concat " && "
            (List.map
               (fun f -> "a." ^ f.field_id ^ " == b." ^ f.field_id)
               s.structure_fields)
        ^ "; "
      else "true;" )
    ^ "} "
    ^ template_parameters_declaration s.structure_parameters
    ^ "bool operator!=(" ^ full_structure_type s ^ " const& a, "
    ^ full_structure_type s ^ " const& b) " ^ "{ return !(a == b); } "
    ^ template_parameters_declaration s.structure_parameters
    ^ "bool operator<(" ^ full_structure_type s ^ " const& a, "
    ^ full_structure_type s ^ " const& b) " ^ "{ "
    ^ ( match s.structure_super with
      | Some super ->
          "if (as_" ^ super ^ "(a) < as_" ^ super ^ "(b)) " ^ "return true; "
          ^ "if (as_" ^ super ^ "(b) < as_" ^ super ^ "(a)) " ^ "return false; "
      | None -> "" )
    ^ String.concat ""
        (List.map
           (fun f ->
             "if (a." ^ f.field_id ^ " < b." ^ f.field_id ^ ") "
             ^ "return true; " ^ "if (b." ^ f.field_id ^ " < a." ^ f.field_id
             ^ ") " ^ "return false; ")
           s.structure_fields)
    ^ "    return false; " ^ "} "

(* Generate the implementations of the C++ comparison operators. *)
let structure_comparison_implementations s =
  if not (has_parameters s) then
    "bool operator==(" ^ s.structure_id ^ " const& a, " ^ s.structure_id
    ^ " const& b) " ^ "{ " ^ "return "
    ^ ( if List.length s.structure_fields > 0 then
        ( match s.structure_super with
        | Some super -> "as_" ^ super ^ "(a) == as_" ^ super ^ "(b) && "
        | None -> "" )
        ^ String.concat " && "
            (List.map
               (fun f -> "a." ^ f.field_id ^ " == b." ^ f.field_id)
               s.structure_fields)
        ^ "; "
      else "true;" )
    ^ "} " ^ "bool operator!=(" ^ s.structure_id ^ " const& a, "
    ^ s.structure_id ^ " const& b) " ^ "{ return !(a == b); } "
    ^ "bool operator<(" ^ s.structure_id ^ " const& a, " ^ s.structure_id
    ^ " const& b) " ^ "{ "
    ^ ( match s.structure_super with
      | Some super ->
          "if (as_" ^ super ^ "(a) < as_" ^ super ^ "(b)) " ^ "return true; "
          ^ "if (as_" ^ super ^ "(b) < as_" ^ super ^ "(a)) " ^ "return false; "
      | None -> "" )
    ^ String.concat ""
        (List.map
           (fun f ->
             "if (a." ^ f.field_id ^ " < b." ^ f.field_id ^ ") "
             ^ "return true; " ^ "if (b." ^ f.field_id ^ " < a." ^ f.field_id
             ^ ") " ^ "return false; ")
           s.structure_fields)
    ^ "    return false; " ^ "} "
  else ""

(* Generate the declaration of the C++ swap function. *)
let structure_swap_declaration s =
  if not (has_parameters s) then
    "void swap(" ^ s.structure_id ^ "& a, " ^ s.structure_id ^ "& b); "
  else
    template_parameters_declaration s.structure_parameters
    ^ "void swap(" ^ full_structure_type s ^ "& a, " ^ full_structure_type s
    ^ "& b) " ^ "{ " ^ "    using std::swap; "
    ^ ( match s.structure_super with
      | Some super -> "swap(as_" ^ super ^ "(a), as_" ^ super ^ "(b)); "
      | None -> "" )
    ^ String.concat ""
        (List.map
           (fun f -> "    swap(a." ^ f.field_id ^ ", b." ^ f.field_id ^ "); ")
           s.structure_fields)
    ^ "} "

(* Generate the implementation of the C++ swap function. *)
let structure_swap_implementation s =
  if not (has_parameters s) then
    "void swap(" ^ full_structure_type s ^ "& a, " ^ full_structure_type s
    ^ "& b) " ^ "{ " ^ "    using std::swap; "
    ^ ( match s.structure_super with
      | Some super -> "swap(as_" ^ super ^ "(a), as_" ^ super ^ "(b)); "
      | None -> "" )
    ^ String.concat ""
        (List.map
           (fun f -> "    swap(a." ^ f.field_id ^ ", b." ^ f.field_id ^ "); ")
           s.structure_fields)
    ^ "} "
  else ""

(* Generate the .hpp code for the deep_sizeof function. *)
let structure_deep_sizeof_declaration s =
  if not (has_parameters s) then
    "size_t deep_sizeof(" ^ s.structure_id ^ " const& x); "
  else
    template_parameters_declaration s.structure_parameters
    ^ "size_t deep_sizeof(" ^ full_structure_type s ^ " const& x) " ^ "{ "
    ^ "    using cradle::deep_sizeof; " ^ "    return 0 "
    ^ ( match s.structure_super with
      | Some super -> "+ deep_sizeof(as_" ^ super ^ "(x)) "
      | None -> "" )
    ^ String.concat ""
        (List.map
           (fun f -> "+ deep_sizeof(x." ^ f.field_id ^ ") ")
           s.structure_fields)
    ^ "; " ^ "} "

(* Generate the .cpp code for the deep_sizeof function. *)
let structure_deep_sizeof_implementation s =
  if not (has_parameters s) then
    "size_t deep_sizeof(" ^ full_structure_type s ^ " const& x) " ^ "{ "
    ^ "    using cradle::deep_sizeof; " ^ "    return 0 "
    ^ ( match s.structure_super with
      | Some super -> "+ deep_sizeof(as_" ^ super ^ "(x)) "
      | None -> "" )
    ^ String.concat ""
        (List.map
           (fun f -> "+ deep_sizeof(x." ^ f.field_id ^ ") ")
           s.structure_fields)
    ^ "; " ^ "} "
  else ""

(* Generate the C++ code for a default constructor and a constructor taking
   a value for each field. Also generate the move constructor and
   assignment operator. *)
let structure_constructor_code s =
  (* default constructor *)
  s.structure_id ^ "() " ^ "{} "
  (* field-by-field constructor *)
  ^ ( if List.length s.structure_fields > 0 then
      (* If there's only one field, we need to add 'explicit' so that C++
         doesn't mistake this for a conversion from the field type. *)
      (if List.length s.structure_fields == 1 then "explicit " else "")
      ^ s.structure_id ^ "("
      ^ ( match s.structure_super with
        | Some super -> super ^ " const& super, "
        | None -> "" )
      ^ String.concat ", "
          (List.map
             (fun f -> cpp_code_for_type f.field_type ^ " const& " ^ f.field_id)
             s.structure_fields)
      ^ ") : "
      ^ ( match s.structure_super with
        | Some super -> super ^ "(super), "
        | None -> "" )
      ^ String.concat ", "
          (List.map
             (fun f -> f.field_id ^ "(" ^ f.field_id ^ ")")
             s.structure_fields)
      ^ " {} "
    else "" )
  (* I think these constructors and assignment operators should be
     unnecessary, but explicitly defining separate versions with move
     semantics helps performance in Visual C++ 11... *)
  (* copy constructor *)
  ^ s.structure_id
  ^ "(" ^ s.structure_id ^ " const& other) "
  ^ ( if List.length s.structure_fields > 0 || structure_has_super s then
      ": "
      ^ ( match s.structure_super with
        | Some super -> super ^ "(static_cast<" ^ super ^ " const&>(other)), "
        | None -> "" )
      ^ String.concat ", "
          (List.map
             (fun f -> f.field_id ^ "(other." ^ f.field_id ^ ")")
             s.structure_fields)
    else "" )
  ^ " {} " (* move constructor *) ^ s.structure_id
  ^ "(" ^ s.structure_id ^ "&& other) "
  ^ ( if List.length s.structure_fields > 0 || structure_has_super s then
      ": "
      ^ ( match s.structure_super with
        | Some super -> super ^ "(static_cast<" ^ super ^ "&&>(other)), "
        | None -> "" )
      ^ String.concat ", "
          (List.map
             (fun f ->
               f.field_id ^ "(" ^ "std::move(other." ^ f.field_id ^ "))")
             s.structure_fields)
    else "" )
  ^ " {} " (* copy assignment operator *) ^ s.structure_id
  ^ "& operator=(" ^ s.structure_id ^ " const& other) " ^ "{ "
  ^ ( match s.structure_super with
    | Some super ->
        "static_cast<" ^ super ^ "&>(*this) = " ^ "static_cast<" ^ super
        ^ " const&>(other); "
    | None -> "" )
  ^ String.concat ""
      (List.map
         (fun f -> "this->" ^ f.field_id ^ " = " ^ "other." ^ f.field_id ^ "; ")
         s.structure_fields)
  ^ "return *this; " ^ "} " (* move assignment operator *) ^ s.structure_id
  ^ "& operator=(" ^ s.structure_id ^ "&& other) " ^ "{ "
  ^ ( match s.structure_super with
    | Some super ->
        "static_cast<" ^ super ^ "&>(*this) = " ^ "static_cast<" ^ super
        ^ "&&>(other); "
    | None -> "" )
  ^ String.concat ""
      (List.map
         (fun f ->
           "this->" ^ f.field_id ^ " = " ^ "std::move(other." ^ f.field_id
           ^ "); ")
         s.structure_fields)
  ^ "return *this; " ^ "} "

(* Generate the full declaration for a structure. *)
let structure_declaration s =
  if structure_is_preexisting s then ""
  else
    template_parameters_declaration s.structure_parameters
    ^ "struct " ^ s.structure_id ^ " "
    ^ ( match s.structure_super with
      | Some super -> ": " ^ super ^ " "
      | None -> "" )
    ^ "{ "
    ^ String.concat " "
        (List.map (fun f -> field_declaration f ^ " ") s.structure_fields)
    ^ " "
    ^ structure_constructor_code s
    ^ "}; "

(* Generate a structure's "make" constructor. *)
let structure_make_constructor_definition s =
  cpp_code_lines
    [
      template_parameters_declaration s.structure_parameters;
      "inline " ^ full_structure_type s;
      "make_" ^ s.structure_id ^ "(";
      ( match s.structure_super with
      | Some super -> super ^ " super, "
      | None -> "" );
      String.concat ", "
        (List.map
           (fun f -> cpp_code_for_type f.field_type ^ " " ^ f.field_id)
           s.structure_fields);
      ")";
      "{";
      "return " ^ full_structure_type s ^ "(";
      ( match s.structure_super with
      | Some super -> "std::move(super), "
      | None -> "" );
      String.concat ", "
        (List.map (fun f -> "std::move(" ^ f.field_id ^ ")") s.structure_fields);
      ");";
      "}";
    ]

(* If a structure has a supertype, a function is generated to automatically
   extract that subset of the structure. *)
let structure_subtyping_definitions env s =
  match s.structure_super with
  | Some super ->
      "inline static " ^ super ^ " const& as_" ^ super ^ "(" ^ s.structure_id
      ^ " const& x) " ^ "{ " ^ "    return static_cast<" ^ super
      ^ " const&>(x); " ^ "} " ^ "inline static " ^ super ^ "& as_" ^ super
      ^ "(" ^ s.structure_id ^ "& x) " ^ "{ " ^ "    return static_cast<"
      ^ super ^ "&>(x); " ^ "} "
  | None -> ""

let structure_hash_declaration namespace s =
  if not (has_parameters s) then
    cpp_code_lines [ "size_t"; "hash_value(" ^ s.structure_id ^ " const& x);" ]
  else
    cpp_code_lines
      [
        template_parameters_declaration s.structure_parameters;
        "size_t";
        "hash_value(" ^ full_structure_type s ^ " const& x)";
        "{";
        "    size_t h = 0;";
        String.concat ""
          (List.map
             (fun f ->
               "boost::hash_combine(h, cradle::invoke_hash(x." ^ f.field_id
               ^ ")); ")
             s.structure_fields);
        "    return h;";
        "}";
      ]

let structure_hash_definition namespace s =
  if not (has_parameters s) then
    cpp_code_lines
      [
        "size_t";
        "hash_value(" ^ s.structure_id ^ " const& x)";
        "{";
        "size_t h = 0; ";
        String.concat ""
          (List.map
             (fun f ->
               "boost::hash_combine(h, cradle::invoke_hash(x." ^ f.field_id
               ^ ")); ")
             s.structure_fields);
        "return h;";
        "}";
      ]
  else ""

(* Generate all the C++ code that needs to appear in the header file for a
   structure. *)
let hpp_string_of_structure app_id app_namespace env s =
  let namespace = resolve_structure_namespace app_namespace s in
  "} namespace " ^ namespace ^ " { " ^ structure_declaration s
  (* ^ structure_request_declaration s *)
  ^ structure_make_constructor_definition s
  ^ structure_type_info_declaration s
  (* ^ structure_upgrade_type_declaration s
     ^ structure_upgrade_value_declaration s *)
  ^ structure_subtyping_definitions env s
  ^ ( if structure_component_is_preexisting s "comparisons" then ""
    else structure_comparison_declarations s )
  ^ structure_swap_declaration s
  ^ structure_deep_sizeof_declaration s
  ^ structure_value_conversion_declarations s
  ^ ( if structure_component_is_preexisting s "iostream" then ""
    else structure_iostream_declarations s )
  ^ structure_hash_declaration namespace s
  ^ "} namespace " ^ app_namespace ^ " { "

(* Generate all the C++ code that needs to appear in the .cpp file for a
   structure. *)
let cpp_string_of_structure account_id app_id app_namespace env s =
  let namespace = resolve_structure_namespace app_namespace s in
  "} namespace " ^ namespace ^ " { "
  (* ^ structure_request_definition s *)
  ^ structure_type_info_definition app_id s
  (* ^ structure_upgrade_type_definition app_id s
     ^ structure_upgrade_value_definition app_id s
     ^ "} namespace " ^ app_namespace ^ " { "
     ^ structure_upgrade_register_function account_id app_id s
     ^ "} namespace " ^ namespace ^ " { " *)
  ^ ( if structure_component_is_preexisting s "comparisons" then ""
    else structure_comparison_implementations s )
  ^ structure_swap_implementation s
  ^ structure_deep_sizeof_implementation s
  ^ structure_value_conversion_definitions s
  ^ ( if structure_component_is_preexisting s "iostream" then ""
    else structure_iostream_definitions s )
  ^ structure_hash_definition namespace s
  ^ "} namespace " ^ app_namespace ^ " { "

(* Generate the C++ code to register a manual structure as part of an API.
   Manual structures are structures that aren't preprocessed but are still
   registered with the API. *)
let cpp_code_to_register_manual_structure app_id cpp_name name revision
    description =
  cpp_code_lines
    [
      "register_api_named_type(";
      "    api,";
      "    \"" ^ name ^ "\",";
      "    " ^ string_of_int revision ^ ",";
      "    \"" ^ String.escaped description ^ "\",";
      "    get_definitive_type_info<" ^ cpp_name ^ ">());";
      (* "    get_upgrade_type(" ^ cpp_name ^ "(), std::vector<std::type_index>())); " *)
    ]

(* Generate the C++ code to register a structure as part of an API. *)
let cpp_code_to_register_structure app_id s =
  if not (structure_is_internal s) then
    let instantiations = enumerate_combinations (get_structure_variants s) in
    String.concat ""
      (List.map
         (fun (assignments, label) ->
           cpp_code_to_register_manual_structure app_id
             ( s.structure_id
             ^ resolved_template_parameter_list assignments
                 s.structure_parameters )
             (s.structure_id ^ label) (get_structure_revision s)
             s.structure_description)
         instantiations)
  else ""

(* Generate the C++ code to register a concrete instance of a record
    (i.e., without template parameters) as part of the API. *)
let cpp_code_to_register_record_instance app_id account name record_name
    description =
  "\n register_api_record_type(api, " ^ "\"" ^ record_name ^ "\", \""
  ^ description ^ "\", " ^ "\"" ^ account ^ "\", \"" ^ app_id ^ "\", \"" ^ name
  ^ "\"); "

(* Generate the C++ code to register a record as part of an API. *)
let cpp_code_to_register_record_from_structure app_id account s =
  if structure_is_record s then
    let instantiations = enumerate_combinations (get_structure_variants s) in
    String.concat ""
      (List.map
         (fun (assignments, label) ->
           cpp_code_to_register_record_instance app_id account
             (s.structure_id ^ label)
             (get_structure_record_name s)
             s.structure_description)
         instantiations)
  else ""

(* Generate C++ code to register API function for upgrading values *)
let cpp_code_to_register_upgrade_function_instance s label =
  let full_public_name = "upgrade_value_" ^ s.structure_id ^ label in
  "\nregister_api_function(api, " ^ "cradle::api_function_ptr(new "
  ^ full_public_name ^ "_fn_def)); "

(* Generate C++ code to register API function for upgrading values *)
let cpp_code_to_register_upgrade_function app_id s =
  if not (structure_is_internal s) then
    let instantiations = enumerate_combinations (get_structure_variants s) in
    String.concat ""
      (List.map
         (fun (assignments, label) ->
           cpp_code_to_register_upgrade_function_instance s label)
         instantiations)
  else ""

(* Generate the C++ code to register a structure and its associated record as
    part of an API. *)
let cpp_code_to_register_structures_and_record app_id account s =
  cpp_code_to_register_structure app_id s
  ^ cpp_code_to_register_record_from_structure app_id account s
  ^ cpp_code_to_register_upgrade_function app_id s
