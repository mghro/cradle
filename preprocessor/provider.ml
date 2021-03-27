(* This file contains code for preprocessing thinknode provider. *)

open Types
open Utilities

(* Generate a function call to register the provider tag *)
let cpp_code_to_register_provider app_id p =
  "\n register_api_provider_type(api, " ^ "\"" ^ p.tag ^ "\"); "

(* Generate a function call to specify the previous release that the data was 
    generated from *)
let cpp_code_to_register_previous_release_version app_id ufv =
  "\n register_api_previous_release_version(api, " ^ "\"" ^ ufv.version
  ^ "\"); "
