%{

open Lexing
open Types

%}

%token LSQUARE RSQUARE LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE
%token COLON DOUBLE_COLON SEMICOLON COMMA EQUALS PLUS MINUS AMPERSAND DOT
%token STRUCT UNION ENUM TYPENAME CONST REVISION API FUNCTION NAME
%token MONITORED TRIVIAL REMOTE EMPTY_COMMENT_STRING TEMPLATE
%token CLASS UNSIGNED WITH ID_KEYWORD INTERNAL LEGACY MANUAL END PREEXISTING
%token UPGRADE MUTATION DEPENDENCY PROVIDER PREVIOUS_RELEASE_VERSION DOUBLEQUOTES RECORD
%token ACCOUNT APP VERSION STRUCTURE REPORTED
%token NAMESPACE PRESERVE_CASE EXECUTION_CLASS REGISTER_ENUM DISK_CACHED LEVEL
%token <string> ID COMMENT_STRING VER
%token <int> INTEGER

%type <Types.unresolved_declaration> single_declaration
%type <Types.unresolved_declaration> declaration
%type <Types.manual_structure> manual_structure_declaration
%type <Types.api_option> api_option
%type <Types.api_option list> api_option_list
%type <Types.api_option list> nonempty_api_option_list
%type <Types.structure> structure
%type <Types.unresolved_mutation> mutation
%type <Types.template_parameter list * string * string option * Types.structure_field list> cpp_structure
%type <Types.structure_field list> structure_field_list
%type <Types.structure_field list> structure_field_group
%type <Types.structure_option> structure_option
%type <Types.structure_option list> structure_option_list
%type <Types.structure_option list> nonempty_structure_option_list
%type <Types.template_parameter list> template_parameter_list
%type <Types.template_parameter list> inner_template_parameter_list
%type <Types.template_parameter> template_parameter
%type <string list> id_list
%type <Types.enum> enum
%type <Types.enum_value list> enum_value_list
%type <Types.enum_value> enum_value
%type <Types.enum_option> enum_option
%type <Types.enum_option list> enum_option_list
%type <Types.enum_option list> nonempty_enum_option_list
%type <Types.union> union
%type <Types.union_member list> union_member_list
%type <Types.union_member> union_member
%type <Types.union_option> union_option
%type <Types.union_option list> union_option_list
%type <Types.union_option list> nonempty_union_option_list
%type <string> comment_string
%type <string> nonempty_comment_string
%type <string> comment_element
%type <string> comment_block
%type <Types.type_string list> type_string_list
%type <Types.type_string list> nonempty_type_string_list
%type <Types.type_string> type_string
%type <Types.unresolved_function_declaration> function_declaration
%type <Types.function_option> function_option
%type <Types.function_option list> function_option_list
%type <Types.function_option list> nonempty_function_option_list
%type <unit> optional_semicolon
%type <Types.template_instantiations list> template_instantiations_list
%type <Types.template_instantiations> template_instantiations
%type <type_string list> template_parameter_value_list
%type <Types.parameter list> parameter_list
%type <Types.parameter list> nonempty_parameter_list
%type <Types.parameter> parameter
%type <bool> parameter_by_reference
%type <string> cpp_id
%type <Types.provider> provider
%type <Types.previous_release_version> previous_release_version
%type <Types.record> record
%type <Types.dependency> dependency
%type <Types.version_string> version_string
%type <Types.string_value> string_value


%start single_declaration

%%

single_declaration:
    declaration END { $1 }

declaration:
    structure { UDstructure $1 }
  | mutation { UDmutation $1 }
  | enum { UDenum $1 }
  | union { UDunion $1 }
  | function_declaration { UDfunction $1 }
  | manual_structure_declaration { UDmanual_structure $1 }
  | provider { UDprovider $1 }
  | previous_release_version { UDprevious_release_version $1 }
  | record { UDrecord $1 }
  | dependency { UDdependency $1 }

manual_structure_declaration:
    comment_string
    API LPAREN STRUCT MANUAL REVISION LPAREN INTEGER RPAREN RPAREN
    STRUCT ID SEMICOLON
    { { manual_structure_id = $12; manual_structure_revision = $8; } }
  | comment_string
    API LPAREN STRUCT MANUAL RPAREN
    STRUCT ID SEMICOLON
    { { manual_structure_id = $8; manual_structure_revision = 0; } }

version_string:
  DOUBLEQUOTES VER DOUBLEQUOTES
  { $2 }

string_value:
  DOUBLEQUOTES ID DOUBLEQUOTES
  { $2 }

api_option:
    STRUCTURE string_value RPAREN { Mstructure $2 }
  | VERSION version_string RPAREN { Mversion $2 }
  | APP string_value RPAREN { Mapp $2 }
  | ACCOUNT string_value RPAREN { Maccount $2 }

api_option_list:
    { [] }
  | nonempty_api_option_list { $1 }

nonempty_api_option_list:
    api_option { [$1] }
  | api_option nonempty_api_option_list { $1::$2 }

provider:
  comment_string API LPAREN PROVIDER version_string RPAREN RPAREN
  {{ tag = $5 }}

previous_release_version:
  comment_string API LPAREN PREVIOUS_RELEASE_VERSION version_string RPAREN RPAREN
  {{ version = $5 }}  

record:
  comment_string API LPAREN RECORD
  NAME LPAREN string_value RPAREN
  api_option_list RPAREN
  {{  description = $1;
      name = $7;
      record_options = $9  }}

dependency:
  comment_string API LPAREN DEPENDENCY api_option_list
  RPAREN
  { { dependency_options = $5; } }

mutation:
  comment_string API LPAREN MUTATION api_option_list RPAREN
  MUTATION ID
  { {  um_mutation_name = $8;
        um_mutation_options = $5;
        um_mutation_structure_description = $1;
        um_mutation_body = None } }

structure:
    comment_string API LPAREN STRUCT structure_option_list RPAREN
    cpp_structure
    { let (parameters, id, super, fields) = $7 in
      { structure_id = id;
        structure_fields = fields;
        structure_description = $1;
        structure_parameters = parameters;
        structure_super = super;
        structure_options = $5 } }

cpp_structure:
    template_parameter_list
    STRUCT ID
    LBRACE structure_field_list RBRACE SEMICOLON
    { ($1, $3, None, $5) }
  | template_parameter_list
    STRUCT ID COLON ID
    LBRACE structure_field_list RBRACE SEMICOLON
    { ($1, $3, Some $5, $7) }

structure_field_list:
    { [] }
  | structure_field_group structure_field_list { (List.append $1 $2) }

structure_field_group:
    comment_string type_string id_list SEMICOLON
    { List.map
        (fun id ->
            { field_id = id;
              field_type = $2;
              field_label = id;
              field_description = $1; })
        $3 }

id_list:
    cpp_id { [$1] }
  | cpp_id COMMA id_list { $1::$3 }

structure_option:
    WITH LPAREN template_instantiations_list RPAREN { SOvariants $3 }
  | NAMESPACE LPAREN ID RPAREN { SOnamespace $3 }
  | PREEXISTING { SOpreexisting [] }
  | PREEXISTING LPAREN id_list RPAREN { SOpreexisting $3 }
  | REVISION LPAREN INTEGER RPAREN { SOrevision $3 }
  | INTERNAL { SOinternal }
  | RECORD LPAREN string_value RPAREN { SOrecord $3 }

structure_option_list:
    { [] }
  | nonempty_structure_option_list { $1 }

nonempty_structure_option_list:
    structure_option { [$1] }
  | structure_option nonempty_structure_option_list { $1::$2 }

template_parameter_list:
    { [] }
  | TEMPLATE LBRACKET inner_template_parameter_list RBRACKET { $3 }

inner_template_parameter_list:
    template_parameter { [$1] }
  | template_parameter COMMA inner_template_parameter_list { $1::$3 }

template_parameter:
    UNSIGNED ID { (Punsigned, $2) }
  | CLASS ID { (Pclass, $2) }
  | TYPENAME ID { (Pclass, $2) }

enum:
    comment_string
    API LPAREN ENUM enum_option_list RPAREN
    ENUM CLASS ID LBRACE enum_value_list RBRACE SEMICOLON
    { { enum_id = $9;
        enum_values = $11;
        enum_description = $1;
        enum_options = $5 } }

enum_value_list:
    { [] }
  | enum_value { [$1] }
  | enum_value COMMA enum_value_list { $1::$3 }

enum_value:
    comment_string ID
    { { ev_id = $2;
        ev_label = $2;
        ev_description = $1; } }

enum_option:
    PRESERVE_CASE { EOpreserve_case }
  | REVISION LPAREN INTEGER RPAREN { EOrevision $3 }
  | INTERNAL { EOinternal }

enum_option_list:
    { [] }
  | nonempty_enum_option_list { $1 }

nonempty_enum_option_list:
    enum_option { [$1] }
  | enum_option nonempty_enum_option_list { $1::$2 }

union:
    comment_string API LPAREN UNION union_option_list RPAREN
    UNION ID LBRACE union_member_list RBRACE SEMICOLON
    { { union_id = $8;
        union_members = $10;
        union_description = $1;
        union_options = $5 } }

union_member_list:
    { [] }
  | union_member union_member_list { $1::$2 }

union_member:
    comment_string type_string cpp_id SEMICOLON
    { { um_id = $3; um_type = $2; um_description = $1; } }

union_option:
    REVISION LPAREN INTEGER RPAREN { UOrevision $3 }
  | REGISTER_ENUM { UOregister_enum }
  | INTERNAL { UOinternal }

union_option_list:
    { [] }
  | nonempty_union_option_list { $1 }

nonempty_union_option_list:
    union_option { [$1] }
  | union_option nonempty_union_option_list { $1::$2 }

comment_string:
    { "" }
  | nonempty_comment_string { $1 }

nonempty_comment_string:
    comment_element { $1 }
  | comment_element nonempty_comment_string { $1 ^ $2 }

comment_element:
    comment_block { $1 }
  | EMPTY_COMMENT_STRING { "\n" }

comment_block:
    COMMENT_STRING { $1 }
  | COMMENT_STRING comment_block { $1 ^ " " ^ $2 }

type_string_list:
    { [] }
  | nonempty_type_string_list { $1 }

nonempty_type_string_list:
    type_string { [$1] }
  | type_string COMMA nonempty_type_string_list { $1::$3 }

type_string:
    TYPENAME type_string { Ttypename::$2 }
  | cpp_id { [Tid $1] }
  | UNSIGNED { [Tid "unsigned"] }
  | type_string DOUBLE_COLON type_string { (List.append $1 (Tseparator::$3)) }
  | INTEGER { [Tinteger $1] }
  | type_string PLUS type_string { [Tsum ($1, $3)] }
  | type_string MINUS type_string { [Tdifference ($1, $3)] }
  | type_string LBRACKET type_string_list RBRACKET
    { (List.append $1 [Tparameters $3]) }

function_declaration:
    comment_string
    API LPAREN FUNCTION function_option_list RPAREN
    template_parameter_list
    comment_string
    type_string ID
    LPAREN parameter_list RPAREN optional_semicolon
    { { ufd_id = $10;
        ufd_description = $1;
        ufd_template_parameters = $7;
        ufd_parameters = $12;
        ufd_return_type = $9;
        ufd_return_description = $8;
        ufd_body = None;
        ufd_options = $5; } }

optional_semicolon:
    SEMICOLON { () }
  | { () }

function_option_list:
    { [] }
  | nonempty_function_option_list { $1 }

nonempty_function_option_list:
    function_option { [$1] }
  | function_option nonempty_function_option_list { $1::$2 }

function_option:
    MONITORED { FOmonitored }
  | TRIVIAL { FOtrivial }
  | REMOTE { FOremote }
  | INTERNAL { FOinternal }
  | REPORTED { FOreported }
  | UPGRADE version_string RPAREN { FOupgrade_version $2 }
  | DISK_CACHED { FOdisk_cached }
  | NAME LPAREN ID RPAREN { FOname $3 }
  | WITH LPAREN template_instantiations_list RPAREN { FOvariants $3 }
  | REVISION LPAREN INTEGER RPAREN { FOrevision $3 }
  | EXECUTION_CLASS LPAREN ID RPAREN { FOexecution_class $3 }
  | LEVEL LPAREN INTEGER RPAREN { FOlevel $3 }

template_instantiations_list:
    template_instantiations { [$1] }
  | template_instantiations SEMICOLON template_instantiations_list { $1::$3 }

template_instantiations:
    ID COLON template_parameter_value_list { ($1, $3) }

template_parameter_value_list:
    type_string { [$1] }
  | type_string COMMA template_parameter_value_list { $1::$3 }

parameter_list:
    { [] }
  | nonempty_parameter_list { $1 }

nonempty_parameter_list:
    parameter { [$1] }
  | parameter COMMA nonempty_parameter_list { $1::$3 }

parameter:
    comment_string type_string parameter_by_reference cpp_id
    { { parameter_id = $4; parameter_description = $1; parameter_type = $2;
        parameter_by_reference = $3 } }

parameter_by_reference:
    { false }
  | CONST AMPERSAND { true }

cpp_id:
    ID { $1 }
  | ID_KEYWORD { "id" }
  | API { "api" }
  | FUNCTION { "fun" }
  | MONITORED { "monitored" }
  | TRIVIAL { "trivial" }
  | REMOTE { "remote" }
  | REVISION { "revision" }
  | NAME { "name" }
  | INTERNAL { "internal" }
  | RECORD { "record" }
  | MUTATION { "mutation" }
  | LEGACY { "legacy" }
  | MANUAL { "manual" }
  | PREEXISTING { "preexisting" }
  | PRESERVE_CASE { "preserve_case" }
  | EXECUTION_CLASS { "execution_class" }
  | REPORTED { "reported" }
  | LEVEL { "level" }

%%
