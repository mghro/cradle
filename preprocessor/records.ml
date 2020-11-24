(* This file contains code for preprocessing Thinknode records. *)

open Types
open Utilities

(* Generate the C++ code to register a concrete instance of a record
        (i.e., without template parameters) as part of the API. *)
let cpp_code_to_register_record_instance app_id account name record_name
    description =
  "\n register_api_record_type(api, " ^ "\"" ^ record_name ^ "\", \""
  ^ description ^ "\", " ^ "\"" ^ account ^ "\", \"" ^ app_id ^ "\", \"" ^ name
  ^ "\"); "

let cpp_code_to_register_record app_id r =
  cpp_code_to_register_record_instance
    (get_app_option r.record_options)
    (get_account_option r.record_options)
    (get_structure_option r.record_options)
    r.name r.description
