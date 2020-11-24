(* This file contains various utilities that are used throughout the
   preprocessor. Mostly, they deal with working with generic C++ types and
   template parameters. *)

open Types

exception DuplicateOption

(* Generate the C++ code for a C++ type. *)
let cpp_code_for_type t =
  let rec code_for_type_component c =
    match c with
    | Ttypename -> "typename "
    | Tid id -> id
    | Tinteger n -> string_of_int n
    | Tparameters p ->
        "<" ^ String.concat "," (List.map (fun t -> code_for_type t) p) ^ " >"
    | Tseparator -> "::"
    | Tsum (a, b) -> code_for_type a ^ "+" ^ code_for_type b
    | Tdifference (a, b) -> code_for_type a ^ "-" ^ code_for_type b
  and code_for_type t =
    String.concat "" (List.map (fun c -> code_for_type_component c) t)
  in
  code_for_type t

(* Given a list of assignments (mapping IDs to C++ types), resolve the actual
   C++ code for the given type ID.
   If id is in the mapping, it resolves to code for the mapped type.
   Otherwise, it resolves to itself. *)
let rec resolve_type_id assignments id =
  match assignments with
  | [] -> id
  | (assigned_id, assigned_t) :: rest ->
      if id = assigned_id then cpp_code_for_type assigned_t
      else resolve_type_id rest id

(* Given a list of variable assignments and a C++ type that may contain
   template variables, this computes the C++ code for that type with all
   the assignments applied. *)
let cpp_code_for_parameterized_type assignments t =
  let rec code_for_parameterized_type_component assignments c =
    match c with
    | Ttypename -> "typename "
    | Tid id -> resolve_type_id assignments id
    | Tinteger n -> string_of_int n
    | Tparameters p ->
        "<"
        ^ String.concat ","
            (List.map (code_for_parameterized_type assignments) p)
        ^ " >"
    | Tseparator -> "::"
    | Tsum (a, b) ->
        code_for_parameterized_type assignments a
        ^ "+"
        ^ code_for_parameterized_type assignments b
    | Tdifference (a, b) ->
        code_for_parameterized_type assignments a
        ^ "-"
        ^ code_for_parameterized_type assignments b
  and code_for_parameterized_type assignments t =
    String.concat ""
      (List.map (code_for_parameterized_type_component assignments) t)
  in
  code_for_parameterized_type assignments t

(* Given a list of template variables, each with a list of possible
   assignments, this computes a list of all combinations of those assignments.
   Each combination is paired with a label that uniquely identifies it. *)
let rec enumerate_combinations variants =
  match variants with
  | [] -> [ ([], "") ]
  | (id, types) :: rest ->
      let subcombos = enumerate_combinations rest in
      List.concat
        (List.map
           (fun t ->
             let this_label =
               (* The only real variants we have right now are
                  dimensionalities, which are always represented
                  by the parameter N. *)
               if id = "N" then "_" ^ cpp_code_for_type t ^ "d" else ""
             in
             List.map
               (fun (assignments, label) ->
                 ((id, t) :: assignments, this_label ^ label))
               subcombos)
           types)

(* Generate the C++ code to declare a list of template parameters. *)
let template_parameters_declaration parameters =
  if List.length parameters != 0 then
    "template<"
    ^ String.concat ", "
        (List.map
           (fun (t, p) ->
             (match t with Punsigned -> "unsigned" | Pclass -> "class")
             ^ " " ^ p)
           parameters)
    ^ "> "
  else ""

(* Generate the C++ code to specify a list of template parameters. *)
let template_parameter_list parameters =
  if List.length parameters != 0 then
    "<" ^ String.concat "," (List.map (fun (t, p) -> p) parameters) ^ ">"
  else ""

(* Given a list of template parameter assignments and a list of template
   parameters, this generates the C++ code for the template parameter list
   with the assignments applied. *)
let resolved_template_parameter_list assignments parameters =
  if List.length parameters != 0 then
    "<"
    ^ String.concat ","
        (List.map (fun (t, p) -> resolve_type_id assignments p) parameters)
    ^ ">"
  else ""

(* Is t the void type? *)
let is_void t = t = [ Tid "void" ]

let get_version_option option_list =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | Mversion _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec version_option options =
    match options with
    | [] -> ""
    | o :: rest -> (
        match o with
        | Mversion r ->
            check_for_duplicates rest;
            r
        | _ -> version_option rest )
  in
  version_option option_list

let get_account_option option_list =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | Maccount _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec account_option options =
    match options with
    | [] -> ""
    | o :: rest -> (
        match o with
        | Maccount r ->
            check_for_duplicates rest;
            r
        | _ -> account_option rest )
  in
  account_option option_list

let get_app_option option_list =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | Mapp _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec app_option options =
    match options with
    | [] -> ""
    | o :: rest -> (
        match o with
        | Mapp r ->
            check_for_duplicates rest;
            r
        | _ -> app_option rest )
  in
  app_option option_list

let get_structure_option option_list =
  let rec check_for_duplicates others =
    match others with
    | [] -> ()
    | o :: rest -> (
        match o with
        | Mstructure _ -> raise DuplicateOption
        | _ -> check_for_duplicates rest )
  in
  let rec structure_option options =
    match options with
    | [] -> ""
    | o :: rest -> (
        match o with
        | Mstructure r ->
            check_for_duplicates rest;
            r
        | _ -> structure_option rest )
  in
  structure_option option_list
