(* This file contains code for preprocessing C++ unions. *)

open Types
open Utilities
open Enums
open Functions

(* An enumeration is automatically generated to represent which member of a
   union is valid. *)
let type_enum_of_union u =
  {
    enum_id = u.union_id ^ "_type";
    enum_values =
      List.map
        (fun m -> { ev_id = m.um_id; ev_label = m.um_id; ev_description = "" })
        u.union_members;
    enum_description = "";
    enum_options = [];
  }

(* Get the revision of the union u. *)
let get_union_revision u =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | UOrevision _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec get_revision_option options =
    match options with
    | [] -> 0
    | o :: rest -> (
        match o with
        | UOrevision v ->
            check_for_duplicates rest;
            v
        | _ -> get_revision_option rest )
  in
  get_revision_option u.union_options

let union_is_internal u =
  List.exists
    (fun o -> match o with UOinternal -> true | _ -> false)
    u.union_options

let union_has_registered_enum u =
  List.exists
    (fun o -> match o with UOregister_enum -> true | _ -> false)
    u.union_options

let cpp_enum_value_of_union_member u m =
  u.union_id ^ "_type::" ^ String.uppercase m.um_id

let union_declaration u =
  "struct " ^ u.union_id ^ " " ^ "{ " (* members *) ^ u.union_id
  ^ "_type type; " ^ "cradle::any contents_; "
  (* default constructor *)
  ^ u.union_id
  ^ "() {} "
  (* I think these constructors and assignment operators should be
     unnecessary, but explicitly defining separate versions with move
     semantics helps performance in Visual C++ 11... *)
  (* copy constructor *)
  ^ u.union_id
  ^ "(" ^ u.union_id ^ " const& other) "
  ^ ": type(other.type), contents_(other.contents_) {} "
  (* move constructor *)
  ^ u.union_id
  ^ "(" ^ u.union_id ^ "&& other) "
  ^ ": type(other.type), contents_(std::move(other.contents_)) {} "
  (* copy assignment operator *)
  ^ u.union_id
  ^ "& operator=(" ^ u.union_id ^ " const& other) " ^ "{ "
  ^ "type = other.type; " ^ "contents_ = other.contents_; " ^ "return *this; "
  ^ "} " (* move assignment operator *) ^ u.union_id
  ^ "& operator=(" ^ u.union_id ^ "&& other) " ^ "{ " ^ "type = other.type; "
  ^ "contents_ = std::move(other.contents_); " ^ "return *this; " ^ "} " ^ "}; "

let union_type_info_declaration u =
  "cradle::raw_type_info get_proper_type_info(" ^ u.union_id ^ "); "
  ^ "cradle::raw_type_info get_type_info(" ^ u.union_id ^ "); "

(* Generate the declaration for getting the upgrade type. *)
let union_upgrade_type_info_declaration u =
  if not (union_is_internal u) then
    "cradle::upgrade_type get_upgrade_type(" ^ u.union_id
    ^ " const&, std::vector<std::type_index> parsed_types); "
  else ""

(* Generate the declaration for function that will upgrade the union. *)
let union_auto_upgrade_value_declaration u =
  if not (union_is_internal u) then
    "void auto_upgrade_value(" ^ u.union_id ^ " *x, cradle::value const& v); "
  else ""

(* Generate the C++ code for API function declaration that will be used to
    upgrade the value if it is needed. *)
let union_upgrade_value_declaration_api_instance app_id u =
  u.union_id ^ " upgrade_value_" ^ u.union_id ^ "(cradle::value const& v);"

let union_type_info_definition app_id u =
  "cradle::raw_type_info get_proper_type_info(" ^ u.union_id ^ ") " ^ "{ "
  ^ "using cradle::get_type_info; "
  ^ "std::vector<cradle::raw_union_member_info> members; "
  ^ String.concat ""
      (List.map
         (fun m ->
           "members.push_back(cradle::raw_union_member_info(" ^ "\"" ^ m.um_id
           ^ "\"," ^ "\""
           ^ String.escaped m.um_description
           ^ "\"," ^ "get_type_info("
           ^ cpp_code_for_type m.um_type
           ^ "()))); ")
         u.union_members)
  ^ "return cradle::raw_type_info(raw_kind::UNION, "
  ^ "any(cradle::raw_union_info( " ^ "\"" ^ u.union_id ^ "\"," ^ "\""
  ^ String.escaped u.union_description
  ^ "\"," ^ "members))); " ^ "} " ^ "cradle::raw_type_info get_type_info("
  ^ u.union_id ^ ") " ^ "{ "
  ^ "return cradle::raw_type_info(raw_kind::NAMED_TYPE_REFERENCE, "
  ^ "any(raw_named_type_reference(\"" ^ app_id ^ "\", \"" ^ u.union_id
  ^ "\"))); " ^ "} "

(* Generate the C++ code to determine the upgrade type *)
let union_upgrade_type_info_definition app_id u =
  if not (union_is_internal u) then
    "cradle::upgrade_type get_upgrade_type(" ^ u.union_id
    ^ " const&, std::vector<std::type_index> parsed_types)" ^ "{ "
    ^ "using cradle::get_explicit_upgrade_type; "
    ^ "using cradle::get_upgrade_type; "
    ^ "cradle::upgrade_type type = get_explicit_upgrade_type(" ^ u.union_id
    ^ "()); "
    ^ String.concat ""
        (List.map
           (fun m ->
             "if(std::find(parsed_types.begin(), parsed_types.end(), \
              std::type_index(typeid("
             ^ cpp_code_for_type m.um_type
             ^ "()))) == parsed_types.end()) { "
             ^ "parsed_types.push_back(std::type_index(typeid("
             ^ cpp_code_for_type m.um_type
             ^ "()))); "
             ^ "type = cradle::merged_upgrade_type(type, get_upgrade_type("
             ^ cpp_code_for_type m.um_type
             ^ "(), parsed_types)); } ")
           u.union_members)
    ^ "return type;" ^ "} "
  else ""

(* Matches up union member to its upgrade function and sets value to upgraded member value *)
let union_auto_upgrade_value_definition app_id u =
  if not (union_is_internal u) then
    "void auto_upgrade_value(" ^ u.union_id ^ " *x, cradle::value const& v)"
    ^ "{ " ^ "auto const& fields = cradle::cast<cradle::value_map>(v); "
    ^ String.concat ""
        (List.map
           (fun m ->
             "{ auto i = fields.find(value(\"" ^ m.um_id ^ "\")); "
             ^ "if (i != fields.end()) " ^ "{ "
             ^ cpp_code_for_type m.um_type
             ^ " ut;" ^ "upgrade_value(&ut, i->second);" ^ u.union_id
             ^ " temp = make_" ^ u.union_id ^ "_with_" ^ m.um_id ^ "(ut); "
             ^ "*x = temp;" ^ "}}")
           u.union_members)
    ^ " }"
  else ""

(* Generate the C++ code for API function that will be used to upgrade the value. *)
let union_upgrade_value_definition_api_instance app_id u =
  u.union_id ^ " upgrade_value_" ^ u.union_id ^ "(cradle::value const& v)" ^ "{"
  ^ u.union_id ^ " x;" ^ "upgrade_value(&x, v); " ^ "return x; " ^ "}"

(* Generate the function definition for API function that is generated to upgrade the union. *)
let construct_union_upgrade_function_options app_id u =
  let make_function_parameter u =
    [
      {
        parameter_id = "v";
        parameter_type = [ Tid "cradle"; Tseparator; Tid "value" ];
        parameter_description = "value to update";
        parameter_by_reference = true;
      };
    ]
  in
  let make_return_type s = [ Tid u.union_id ] in
  {
    function_variants = [];
    function_id = "upgrade_value_" ^ u.union_id;
    function_description = "upgrade union function for " ^ u.union_id;
    function_template_parameters = [];
    function_parameters = make_function_parameter u;
    function_return_type = make_return_type u;
    function_return_description = "upgraded union value for " ^ u.union_id;
    function_body = None;
    function_has_monitoring = false;
    function_is_trivial = false;
    function_is_remote = true;
    function_is_internal = false;
    function_is_disk_cached = false;
    function_is_reported = false;
    function_revision = 0;
    function_public_name = "upgrade_value_" ^ u.union_id;
    function_execution_class = "cpu.x1";
    function_upgrade_version = "0.0.0";
    function_level = 1;
  }

(* Make call to register API function for upgrading value for union. *)
let union_upgrade_register_function_instance account_id app_id u =
  if not (union_is_internal u) then
    let f = construct_union_upgrade_function_options app_id u in
    cpp_code_to_define_function_instance account_id app_id f "" []
  else ""

let union_constructor_declarations u =
  String.concat ""
    (List.map
       (fun m ->
         u.union_id ^ " make_" ^ u.union_id ^ "_with_" ^ m.um_id ^ "("
         ^ cpp_code_for_type m.um_type
         ^ " const& x); " (* Same, but with move semantics. *) ^ u.union_id
         ^ " make_" ^ u.union_id ^ "_with_" ^ m.um_id ^ "("
         ^ cpp_code_for_type m.um_type
         ^ "&& x); ")
       u.union_members)

let union_constructor_definitions u =
  String.concat ""
    (List.map
       (fun m ->
         u.union_id ^ " make_" ^ u.union_id ^ "_with_" ^ m.um_id ^ "("
         ^ cpp_code_for_type m.um_type
         ^ " const& x) " ^ "{ " ^ u.union_id ^ " s; " ^ "s.type = "
         ^ cpp_enum_value_of_union_member u m
         ^ "; " ^ "s.contents_ = x; " ^ "return s; " ^ "} "
         (* Same, but with move semantics. *)
         ^ u.union_id
         ^ " make_" ^ u.union_id ^ "_with_" ^ m.um_id ^ "("
         ^ cpp_code_for_type m.um_type
         ^ "&& x) " ^ "{ " ^ u.union_id ^ " s; " ^ "s.type = "
         ^ cpp_enum_value_of_union_member u m
         ^ "; " ^ "s.contents_ = std::move(x); " ^ "return s; " ^ "} ")
       u.union_members)

let union_accessor_declarations u =
  String.concat ""
    (List.map
       (fun m ->
         "bool static inline is_" ^ m.um_id ^ "(" ^ u.union_id ^ " const& x) { "
         ^ "return x.type == "
         ^ cpp_enum_value_of_union_member u m
         ^ "; " ^ "} ")
       u.union_members)
  ^ String.concat ""
      (List.map
         (fun m ->
           cpp_code_for_type m.um_type
           ^ " const& as_" ^ m.um_id ^ "(" ^ u.union_id ^ " const& x); ")
         u.union_members)
  ^ String.concat ""
      (List.map
         (fun m ->
           cpp_code_for_type m.um_type
           ^ "& as_" ^ m.um_id ^ "(" ^ u.union_id ^ "& x); ")
         u.union_members)
  ^ String.concat ""
      (List.map
         (fun m ->
           "void set_to_" ^ m.um_id ^ "(" ^ u.union_id ^ "& x, "
           ^ cpp_code_for_type m.um_type
           ^ " const& y); " (* Same, but with move semantics. *)
           ^ "void set_to_" ^ m.um_id ^ "(" ^ u.union_id ^ "& x, "
           ^ cpp_code_for_type m.um_type
           ^ "&& y); ")
         u.union_members)

let union_accessor_definitions u =
  String.concat ""
    (List.map
       (fun m ->
         cpp_code_for_type m.um_type
         ^ " const& as_" ^ m.um_id ^ "(" ^ u.union_id ^ " const& x) " ^ "{ "
         ^ "assert(x.type == "
         ^ cpp_enum_value_of_union_member u m
         ^ "); " ^ "return unsafe_any_cast<"
         ^ cpp_code_for_type m.um_type
         ^ ">(" ^ "x.contents_); " ^ "} ")
       u.union_members)
  ^ String.concat ""
      (List.map
         (fun m ->
           cpp_code_for_type m.um_type
           ^ "& as_" ^ m.um_id ^ "(" ^ u.union_id ^ "& x) " ^ "{ "
           ^ "assert(x.type == "
           ^ cpp_enum_value_of_union_member u m
           ^ "); " ^ "return unsafe_any_cast<"
           ^ cpp_code_for_type m.um_type
           ^ ">(" ^ "x.contents_); " ^ "} ")
         u.union_members)
  ^ String.concat ""
      (List.map
         (fun m ->
           "void set_to_" ^ m.um_id ^ "(" ^ u.union_id ^ "& x, "
           ^ cpp_code_for_type m.um_type
           ^ " const& y) " ^ "{ " ^ "x.type = "
           ^ cpp_enum_value_of_union_member u m
           ^ "; " ^ "x.contents_ = y; " ^ "} "
           (* Same, but with move semantics. *)
           ^ "void set_to_"
           ^ m.um_id ^ "(" ^ u.union_id ^ "& x, "
           ^ cpp_code_for_type m.um_type
           ^ "&& y) " ^ "{ " ^ "x.type = "
           ^ cpp_enum_value_of_union_member u m
           ^ "; " ^ "x.contents_ = std::move(y); " ^ "} ")
         u.union_members)

let union_deep_sizeof_declaration u =
  "size_t deep_sizeof(" ^ u.union_id ^ " const& x); "

let union_deep_sizeof_definition u =
  "size_t deep_sizeof(" ^ u.union_id ^ " const& x) " ^ "{ "
  ^ "using cradle::deep_sizeof; " ^ "size_t size = sizeof(x); "
  ^ "switch (x.type) " ^ "{ "
  ^ String.concat ""
      (List.map
         (fun m ->
           "case "
           ^ cpp_enum_value_of_union_member u m
           ^ ": " ^ "size += deep_sizeof(as_" ^ m.um_id ^ "(x)); " ^ "break; ")
         u.union_members)
  ^ "} " ^ "return size; " ^ "} "

let union_conversion_declarations u =
  "void to_value(cradle::value* v, " ^ u.union_id ^ " const& x); "
  ^ "void from_value(" ^ u.union_id ^ "* x, cradle::value const& v); "
  ^ "std::ostream& operator<<(std::ostream& s, " ^ u.union_id ^ " const& x); "

let union_conversion_definitions u =
  "void to_value(cradle::value* v, " ^ u.union_id ^ " const& x) " ^ "{ "
  ^ "cradle::value_map s; " ^ "switch (x.type) " ^ "{ "
  ^ String.concat ""
      (List.map
         (fun m ->
           "case "
           ^ cpp_enum_value_of_union_member u m
           ^ ": " ^ "to_value(&s[value(\"" ^ m.um_id ^ "\")], as_" ^ m.um_id
           ^ "(x)); " ^ "break; ")
         u.union_members)
  ^ "} " ^ "v->swap_in(s); " ^ "} " ^ "void from_value(" ^ u.union_id
  ^ "* x, cradle::value const& v) " ^ "{ " ^ "cradle::value_map const& s = "
  ^ "cradle::cast<cradle::value_map>(v); "
  ^ "from_value(&x->type, get_union_value_type(s)); " ^ "switch (x->type) "
  ^ "{ "
  ^ String.concat ""
      (List.map
         (fun m ->
           "case "
           ^ cpp_enum_value_of_union_member u m
           ^ ": " ^ " { "
           ^ cpp_code_for_type m.um_type
           ^ " tmp; " ^ "from_value(&tmp, get_field(s, \"" ^ m.um_id ^ "\")); "
           ^ "x->contents_ = tmp; " ^ "break; " ^ " } ")
         u.union_members)
  ^ "} " ^ "} " ^ "std::ostream& operator<<(std::ostream& s, " ^ u.union_id
  ^ " const& x) " ^ "{ return generic_ostream_operator(s, x); } "

let union_swap_declaration u =
  "void swap(" ^ u.union_id ^ "& a, " ^ u.union_id ^ "& b); "

let union_swap_definition u =
  "void swap(" ^ u.union_id ^ "& a, " ^ u.union_id ^ "& b) " ^ "{ "
  ^ "using cradle::swap; " ^ "swap(a.type, b.type); "
  ^ "swap(a.contents_, b.contents_); " ^ "} "

let union_comparison_declarations u =
  "bool operator==(" ^ u.union_id ^ " const& a, " ^ u.union_id ^ " const& b); "
  ^ "bool operator!=(" ^ u.union_id ^ " const& a, " ^ u.union_id
  ^ " const& b); " ^ "bool operator<(" ^ u.union_id ^ " const& a, " ^ u.union_id
  ^ " const& b); "

let union_comparison_definitions u =
  "bool operator==(" ^ u.union_id ^ " const& a, " ^ u.union_id ^ " const& b) "
  ^ "{ " ^ "if (a.type != b.type) " ^ "return false; " ^ "switch (a.type) "
  ^ "{ "
  ^ String.concat ""
      (List.map
         (fun m ->
           "case "
           ^ cpp_enum_value_of_union_member u m
           ^ ": " ^ "return as_" ^ m.um_id ^ "(a) == as_" ^ m.um_id ^ "(b); ")
         u.union_members)
  ^ "} " ^ "return true; " ^ "} " ^ "bool operator!=(" ^ u.union_id
  ^ " const& a, " ^ u.union_id ^ " const& b) " ^ "{ return !(a == b); } "
  ^ "bool operator<(" ^ u.union_id ^ " const& a, " ^ u.union_id ^ " const& b) "
  ^ "{ " ^ "if (a.type < b.type) " ^ "return true; " ^ "if (b.type < a.type) "
  ^ "return false; " ^ "switch (a.type) " ^ "{ "
  ^ String.concat ""
      (List.map
         (fun m ->
           "case "
           ^ cpp_enum_value_of_union_member u m
           ^ ": " ^ "return as_" ^ m.um_id ^ "(a) < as_" ^ m.um_id ^ "(b); ")
         u.union_members)
  ^ "} " ^ "return false; " ^ "} "

let union_hash_declarations namespace u =
  "} namespace std { " ^ "template<> " ^ "struct hash<" ^ namespace ^ "::"
  ^ u.union_id ^ " > " ^ "{ " ^ "size_t operator()(" ^ namespace ^ "::"
  ^ u.union_id ^ " const& x) const; " ^ "}; " ^ "} namespace " ^ namespace
  ^ " { "

let union_hash_definitions namespace u =
  "} namespace std { " ^ "size_t hash<" ^ namespace ^ "::" ^ u.union_id ^ " >::"
  ^ "operator()(" ^ namespace ^ "::" ^ u.union_id ^ " const& x) const " ^ "{ "
  ^ "switch (x.type) " ^ "{ "
  ^ String.concat ""
      (List.map
         (fun m ->
           "case " ^ namespace ^ "::"
           ^ cpp_enum_value_of_union_member u m
           ^ ": " ^ "return cradle::invoke_hash(as_" ^ m.um_id ^ "(x)); ")
         u.union_members)
  ^ "} " ^ "assert(0); return 0; " ^ "} " ^ "} namespace " ^ namespace ^ " { "

(* Generate C++ code to register API function for upgrading values *)
let cpp_code_to_register_upgrade_function_instance u =
  let full_public_name = "upgrade_value_" ^ u.union_id in
  "\nregister_api_function(api, " ^ "cradle::api_function_ptr(new "
  ^ full_public_name ^ "_fn_def)); "

let hpp_string_of_union account_id app_id namespace u =
  hpp_string_of_enum account_id app_id namespace (type_enum_of_union u)
  ^ union_declaration u
  ^ union_type_info_declaration u
  ^ union_constructor_declarations u
  ^ union_accessor_declarations u
  ^ union_comparison_declarations u
  ^ union_hash_declarations namespace u
  ^ union_swap_declaration u
  ^ union_conversion_declarations u
  ^ union_deep_sizeof_declaration u
  ^ union_upgrade_type_info_declaration u
  ^ union_auto_upgrade_value_declaration u
  ^ union_upgrade_value_declaration_api_instance app_id u

let cpp_string_of_union account_id app_id namespace u =
  cpp_string_of_enum account_id app_id namespace (type_enum_of_union u)
  ^ union_type_info_definition app_id u
  ^ union_constructor_definitions u
  ^ union_accessor_definitions u
  ^ union_comparison_definitions u
  ^ union_hash_definitions namespace u
  ^ union_swap_definition u
  ^ union_conversion_definitions u
  ^ union_deep_sizeof_definition u
  ^ union_upgrade_type_info_definition app_id u
  ^ union_auto_upgrade_value_definition app_id u
  ^ union_upgrade_value_definition_api_instance app_id u
  ^ union_upgrade_register_function_instance account_id app_id u

let cpp_code_to_register_union app_id u =
  if not (union_is_internal u) then
    ( if union_has_registered_enum u then
      cpp_code_to_register_enum app_id (type_enum_of_union u)
    else "" )
    ^ "\nregister_api_named_type(api, " ^ "\"" ^ u.union_id ^ "\", "
    ^ string_of_int (get_union_revision u)
    ^ ", " ^ "\""
    ^ String.escaped u.union_description
    ^ "\", " ^ "make_api_type_info(get_proper_type_info(" ^ u.union_id
    ^ "())), " ^ "get_upgrade_type(" ^ u.union_id
    ^ "(), std::vector<std::type_index>())); \n"
    ^ cpp_code_to_register_upgrade_function_instance u
  else ""
