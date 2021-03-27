{
  open Grammar
  open Lexing
}

let digit = ['0'-'9']

(* identifiers - The '.' is in here because some thinknode IDs use them.
   This makes the lexer overly permissive for C++ IDs, but that's fine. *)
let ident = ['a'-'z' 'A'-'Z' '_' '.']
let ident_num = ['a'-'z' 'A'-'Z' '0'-'9' '_' '.']

rule token = parse
    (* Ignore whitespace. *)
  | [' ' '\t' '\n' '\r']
                    { token lexbuf }

  (* These are actual comments and are ignored. *)
  | ("///" [^'\n''\r']*)
                    { token lexbuf }

  (* C++ comments are taken as descriptions and are recorded. *)
  | ("// " (([^'#''\n''\r'][^'\n''\r']*) as string))
                    { COMMENT_STRING string }
  | ("//" ['\n''\r'])
                    { EMPTY_COMMENT_STRING }
  | (digit+ '.' digit+ '.' digit+ '-' ident_num*) as v { VER v }

  | (digit+) as n   { INTEGER (int_of_string n) }

  | '['             { LSQUARE }
  | ']'             { RSQUARE }
  | '{'             { LBRACE }
  | '}'             { RBRACE }
  | '<'             { LBRACKET }
  | '>'             { RBRACKET }
  | '('             { LPAREN }
  | ')'             { RPAREN }

  | ":"             { COLON }
  | "::"            { DOUBLE_COLON }
  | ';'             { SEMICOLON }
  | ','             { COMMA }
  | '='             { EQUALS }
  | '+'             { PLUS }
  | '-'             { MINUS }
  | '&'             { AMPERSAND }
  | '.'             { DOT }
  | '"'             { DOUBLEQUOTES }

  | "api"           { API }
  | "id"            { ID_KEYWORD }
  | "fun"           { FUNCTION }
  | "struct"        { STRUCT }
  | "union"         { UNION }
  | "enum"          { ENUM }
  | "typename"      { TYPENAME }
  | "const"         { CONST }
  | "revision"      { REVISION }
  | "monitored"     { MONITORED }
  | "trivial"       { TRIVIAL }
  | "remote"        { REMOTE }
  | "template"      { TEMPLATE }
  | "class"         { CLASS }
  | "unsigned"      { UNSIGNED }
  | "name"          { NAME }
  | "with"          { WITH }
  | "internal"      { INTERNAL }
  | "upgrade("      { UPGRADE }
  | "mutation"      { MUTATION }
  | "account("      { ACCOUNT }
  | "app("          { APP }
  | "version("      { VERSION }
  | "previous_release("      
                    { PREVIOUS_RELEASE_VERSION }
  | "structure("    { STRUCTURE }
  | "dependency"    { DEPENDENCY }
  | "provider("     { PROVIDER }
  | "record"        { RECORD }
  | "legacy"        { LEGACY }
  | "manual"        { MANUAL }
  | "preexisting"   { PREEXISTING }
  | "preserve_case" { PRESERVE_CASE }
  | "execution_class"
                    { EXECUTION_CLASS }
  | "namespace"     { NAMESPACE }
  | "register_enum" { REGISTER_ENUM }
  | "disk_cached"   { DISK_CACHED }
  | "reported"      { REPORTED }
  | "level"         { LEVEL }

  | (ident ident_num*) as id
                    { ID id }

  | eof             { END }
