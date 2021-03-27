(* This file contains code for preprocessing thinknode dependencies. *)

open Types
open Utilities

let cpp_code_to_register_dependency app_id d =
  "\n register_api_dependency_type(api, " ^ "\""
  ^ get_account_option d.dependency_options
  ^ "\",\n    \""
  ^ get_app_option d.dependency_options
  ^ "\", " ^ "\""
  ^ get_version_option d.dependency_options
  ^ "\"); "
