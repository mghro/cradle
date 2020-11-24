(* This file deals with parsing blocks of the preprocessed file. *)

open Types
open Utilities
open Functions
open Mutations

(* Read a file as a list of line (each stored as a string).
   The list is returned in reverse because that's how it's parsed and that
   happens to be how it's needed by group_lines_into_blocks. *)
let read_file_as_lines file_path =
  let lines = ref [] in
  let chan = open_in file_path in
  try
    while true do
      lines := input_line chan :: !lines
    done;
    []
  with End_of_file ->
    close_in chan;
    !lines

(* Given a reversed list of lines representing a C++ file, this groups the
   lines into blocks. Blocks are delimited by blank lines EXCEPT when there
   are at least two open curly braces.

   (The reason it requires two is that requiring only one would group entire
   namespaces together as single blocks, which is not what we want. Currently,
   all API blocks are nested within one level of namespace (cradle), so
   requiring two braces works. However, in the future, it'd be nice to make
   this more robust.)
*)
let group_lines_into_blocks lines =
  let append_block b line_number blocks =
    match b with [] -> blocks | block -> (line_number, block) :: blocks
  in

  let count_character_occurrences c line =
    let rec aux position n =
      try
        let next = String.index_from line position c in
        aux (next + 1) (n + 1)
      with Not_found -> n
    in
    aux 0 0
  in

  (* Since we're going in reverse, closing braces increase the count and
     open braces decrease it. *)
  let count_braces line =
    count_character_occurrences '}' line - count_character_occurrences '{' line
  in

  let rec group_lines blocks current_block brace_count lines line_number =
    match lines with
    | [] -> append_block current_block 1 blocks
    | current_line :: rest ->
        if
          Str.string_match (Str.regexp "^[\t' '\r\n]*$") current_line 0
          && brace_count < 2
        then
          group_lines
            (append_block current_block (line_number + 1) blocks)
            [] brace_count rest (line_number - 1)
        else
          group_lines blocks
            (current_line :: current_block)
            (brace_count + count_braces current_line)
            rest (line_number - 1)
  in

  group_lines [] [] 0 lines (List.length lines)

exception MissingGuardString

let rec extract_guard_string lines =
  match lines with
  | line :: rest ->
      let regexp = Str.regexp "^#define[ ]+\\([A-Z0-9_]+_HPP\\)[ ]*$" in
      if Str.string_match regexp line 0 then Str.matched_group 1 line
      else extract_guard_string rest
  | [] -> raise MissingGuardString

exception InternalError

let parse_block block =
  let block_text = String.concat "\n" block in
  (* If parsing a function, then separate it into declaration and
     body (which isn't parsed). *)
  try
    if List.exists (fun s -> Str.string_match (Str.regexp "^api(fun") s 0) block
    then
      try
        (* The body is everything from the first '{' on. *)
        let body_offset = String.index block_text '{' in
        let decl_text = String.sub block_text 0 body_offset in
        let body_text =
          String.sub block_text body_offset
            (String.length block_text - body_offset)
        in
        let lexbuf = Lexing.from_string decl_text in
        let decl = Grammar.single_declaration Lexer.token lexbuf in
        match decl with
        | UDfunction f ->
            Dfunction
              (resolve_function_options { f with ufd_body = Some body_text })
        | _ -> raise InternalError
      with Not_found -> (
        (* No body, so just parse it normally. *)
        let lexbuf = Lexing.from_string block_text in
        let decl = Grammar.single_declaration Lexer.token lexbuf in
        match decl with
        | UDfunction f -> Dfunction (resolve_function_options f)
        | _ -> raise InternalError )
    else if
      List.exists
        (fun s -> Str.string_match (Str.regexp "^api(mutation") s 0)
        block
    then
      try
        (* The body is everything from the first '{' on. *)
        let body_offset = String.index block_text '{' in
        let decl_text = String.sub block_text 0 body_offset in
        let body_text =
          String.sub block_text body_offset
            (String.length block_text - body_offset)
        in
        let lexbuf = Lexing.from_string decl_text in
        let decl = Grammar.single_declaration Lexer.token lexbuf in
        match decl with
        | UDmutation m ->
            Dmutation
              (resolve_mutation_options
                 { m with um_mutation_body = Some body_text })
        | _ -> raise InternalError
      with Not_found -> (
        (* No body, so just parse it normally. *)
        let lexbuf = Lexing.from_string block_text in
        let decl = Grammar.single_declaration Lexer.token lexbuf in
        match decl with
        | UDmutation m ->
            Dmutation
              (resolve_mutation_options
                 { m with um_mutation_body = Some "empty body" })
        | _ -> raise InternalError )
    else
      let lexbuf = Lexing.from_string block_text in
      let decl = Grammar.single_declaration Lexer.token lexbuf in
      match decl with
      | UDstructure s -> Dstructure s
      | UDenum e -> Denum e
      | UDunion u -> Dunion u
      | UDmutation _ -> raise InternalError
      | UDfunction _ -> raise InternalError
      | UDmanual_structure s -> Dmanual_structure s
      | UDprovider p -> Dprovider p
      | UDprevious_release_version ufv -> Dprevious_release_version ufv
      | UDrecord r -> Drecord r
      | UDdependency d -> Ddependency d
  with
  | Parsing.Parse_error ->
      prerr_string
        ("error parsing block:\n---\n" ^ String.concat "\n" block ^ "\n---\n");
      raise Parsing.Parse_error
  | Failure f ->
      prerr_string
        ( "failure parsing block:\n---\n" ^ f ^ String.concat "\n" block
        ^ "\n---\n" );
      raise Parsing.Parse_error
