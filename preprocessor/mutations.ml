(* This file contains code for preprocessing thinknode mutation upgrades. *)

open Types
open Utilities

let resolve_mutation_options m =
  let rec get_mutation_version_option options =
    let rec check_for_duplicates others =
      match others with
      | [] -> ()
      | o :: rest -> (
          match o with
          | Mversion _ -> raise DuplicateOption
          | _ -> check_for_duplicates rest )
    in
    match options with
    | [] -> "default_version"
    | o :: rest -> (
        match o with
        | Mversion v ->
            check_for_duplicates rest;
            v
        | _ -> get_mutation_version_option rest )
  in

  let rec get_mutation_structure_option options =
    let rec check_for_duplicates others =
      match others with
      | [] -> ()
      | o :: rest -> (
          match o with
          | Mstructure _ -> raise DuplicateOption
          | _ -> check_for_duplicates rest )
    in
    match options with
    | [] -> "default_struct"
    | o :: rest -> (
        match o with
        | Mstructure s ->
            check_for_duplicates rest;
            s
        | _ -> get_mutation_structure_option rest )
  in
  {
    mutation_structure = get_mutation_structure_option m.um_mutation_options;
    mutation_from_version = get_mutation_version_option m.um_mutation_options;
    mutation_body = m.um_mutation_body;
    mutation_structure_description = m.um_mutation_structure_description;
    mutation_name = m.um_mutation_name;
  }

let remove_linebreaks body_text =
  Str.global_replace (Str.regexp_string "\n") "" body_text

let escape_double_quotes body_text =
  Str.global_replace (Str.regexp_string "\"") "\\\"" body_text

let cpp_code_of_mutation_body m =
  match m.mutation_body with Some code -> code | _ -> "; "

let mutation_item_name prefix n = prefix ^ "_" ^ n

let cpp_code_to_register_mutation app_id m =
  "\nregister_api_mutation_type(api, " ^ "\"" ^ m.mutation_structure_description
  ^ "\", " ^ "\"" ^ m.mutation_from_version ^ "\", \"" ^ m.mutation_structure
  ^ "\", \""
  ^ escape_double_quotes (remove_linebreaks (cpp_code_of_mutation_body m))
  ^ "\"); "
