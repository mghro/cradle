(* This file defines the types used to model the grammar understood by the
   preprocessor. *)

(* type_component and type_string are used to represent arbitrary C++ types.
   type_string corresponds to a full C++ type.
   The preprocessor only attempts to understand enough about C++ types so that
   it can distinguish them from other parts of the grammar during parsing, so
   it represents them as simple strings of components. *)
type type_component =
  | Ttypename
  | Tid of string
  | Tinteger of int
  | Tparameters of type_string list
  | Tseparator
  | Tsum of type_string * type_string
  | Tdifference of type_string * type_string

and type_string = type_component list

(* template_parameter represents a C++ template parameter. *)
type template_parameter_type = Punsigned | Pclass

type template_parameter = template_parameter_type * string

(* This represents a single template parameter and a list of possible values
   for that parameter. *)
type template_instantiations = string * type_string list

(* The following represent C++ structures. *)

type structure_option =
  | SOvariants of template_instantiations list
  | SOpreexisting of string list
  | SOrevision of int
  | SOnamespace of string
  | SOinternal
  | SOrecord of string

type structure_field = {
  field_id : string;
  field_type : type_string;
  field_label : string;
  field_description : string;
}

type structure = {
  structure_id : string;
  structure_fields : structure_field list;
  structure_description : string;
  structure_parameters : template_parameter list;
  structure_super : string option;
  structure_options : structure_option list;
}

(* The following represent Thinknode upgrade mutations. *)

type version_string = string

type string_value = string

type api_option =
  | Mstructure of string
  | Mversion of string
  | Maccount of string
  | Mapp of string

type unresolved_mutation = {
  um_mutation_name : string;
  um_mutation_structure_description : string;
  um_mutation_options : api_option list;
  um_mutation_body : string option;
}

type mutation = {
  mutation_name : string;
  mutation_structure_description : string;
  mutation_structure : string;
  mutation_from_version : string;
  mutation_body : string option;
}

type manual_structure = {
  manual_structure_id : string;
  manual_structure_revision : int;
}

(* The following represents Thinknode dependencies. *)

type dependency = { dependency_options : api_option list }

(* The following represents Thinknode provider. *)

type provider = { tag : string }

(* The following represents version an app will upgrade data from. *)

type previous_release_version = { version : string }

type record = {
  name : string;
  description : string;
  record_options : api_option list;
}

(* The following represent C++ enumerations. *)

type enum_option = EOpreserve_case | EOrevision of int | EOinternal

type enum_value = { ev_id : string; ev_label : string; ev_description : string }

type enum = {
  enum_id : string;
  enum_values : enum_value list;
  enum_description : string;
  enum_options : enum_option list;
}

(* The following represent C++ unions. *)

type union_option = UOrevision of int | UOinternal | UOregister_enum

type union_member = {
  um_id : string;
  um_type : type_string;
  um_description : string;
}

type union = {
  union_id : string;
  union_members : union_member list;
  union_description : string;
  union_options : union_option list;
}

(* The following represent C++ functions.
   Unlike other components, functions have two separate representations:
   resolved and unresolved.
   The unresolved version is what's parsed from the preprocessed file.
   The resolved version is what's used to generate the output.
   (The reason for the distinction is more historical than anything else.) *)

type parameter = {
  parameter_id : string;
  parameter_type : type_string;
  parameter_description : string;
  parameter_by_reference : bool;
}

type function_option =
  | FOvariants of template_instantiations list
  | FOmonitored
  | FOtrivial
  | FOremote
  | FOinternal
  | FOupgrade_version of string
  | FOdisk_cached
  | FOrevision of int
  | FOname of string
  | FOexecution_class of string
  | FOreported
  | FOlevel of int

type unresolved_function_declaration = {
  ufd_id : string;
  ufd_description : string;
  ufd_template_parameters : template_parameter list;
  ufd_parameters : parameter list;
  ufd_return_type : type_string;
  ufd_return_description : string;
  ufd_body : string option;
  ufd_options : function_option list;
}

type function_declaration = {
  function_variants : template_instantiations list;
  function_id : string;
  function_description : string;
  function_template_parameters : template_parameter list;
  function_parameters : parameter list;
  function_return_type : type_string;
  function_return_description : string;
  function_body : string option;
  function_has_monitoring : bool;
  function_is_trivial : bool;
  function_is_remote : bool;
  function_is_internal : bool;
  function_is_disk_cached : bool;
  function_is_reported : bool;
  function_revision : int;
  function_public_name : string;
  function_execution_class : string;
  function_upgrade_version : string;
  function_level : int;
}

(* The following represent the union of all preprocessor declarations.
   Because of the distinction between resolved and unresolved functions, there
   is also a distinction for declarations. *)

type unresolved_declaration =
  | UDstructure of structure
  | UDmutation of unresolved_mutation
  | UDenum of enum
  | UDunion of union
  | UDfunction of unresolved_function_declaration
  | UDmanual_structure of manual_structure
  | UDprovider of provider
  | UDprevious_release_version of previous_release_version
  | UDrecord of record
  | UDdependency of dependency

type declaration =
  | Dstructure of structure
  | Dmutation of mutation
  | Denum of enum
  | Dunion of union
  | Dfunction of function_declaration
  | Dmanual_structure of manual_structure
  | Ddependency of dependency
  | Dprovider of provider
  | Dprevious_release_version of previous_release_version
  | Drecord of record
