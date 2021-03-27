cmake_minimum_required(VERSION 3.0)

include(CMakeParseArguments)

find_program(OCAML_EXECUTABLE ocamlc)
find_program(OCAML_YACC_EXECUTABLE ocamlyacc)
find_program(OCAML_LEX_EXECUTABLE ocamllex)

function(add_ocaml_source object_file ocaml_file dependencies)
    cmake_parse_arguments(MY "SEPARATE_INTERFACE" "" "" ${ARGN})
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")
    string(REGEX REPLACE "[.]cmo" "" without_extension ${object_file})
    if (MY_SEPARATE_INTERFACE)
        set(interface_file "")
    else()
        set(interface_file ${without_extension}.cmi)
    endif()
    set(interface_dependencies)
    foreach(dependency ${dependencies})
        list(APPEND interface_dependencies "${build_dir}/${dependency}.cmi")
    endforeach()
    add_custom_command(
        OUTPUT ${object_file} ${interface_file}
        COMMAND ${OCAML_EXECUTABLE} -c ${ocaml_file}
        DEPENDS ${ocaml_file} ${interface_dependencies}
        WORKING_DIRECTORY ${build_dir}
        COMMENT "compiling OCaml file ${ocaml_file}"
        VERBATIM)
endfunction()

function(add_regular_ocaml_source ocaml_file)
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")
    cmake_parse_arguments(MY "" "" "DEPENDENCIES" ${ARGN})
    get_filename_component(base_name ${ocaml_file} NAME_WE)
    set(copied_file "${build_dir}/${base_name}.ml")
    set(object_file "${build_dir}/${base_name}.cmo")
    add_custom_command(
        OUTPUT ${copied_file}
        COMMAND ${CMAKE_COMMAND} -E copy ${ocaml_file} ${build_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${copied_file}
        DEPENDS ${ocaml_file}
        COMMENT "copying OCaml file ${copied_file}"
        VERBATIM)
    add_ocaml_source(${object_file} ${copied_file} "${MY_DEPENDENCIES}")
endfunction()

function(add_ocaml_yacc_source yacc_file)
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")
    cmake_parse_arguments(MY "" "" "DEPENDENCIES" ${ARGN})
    get_filename_component(base_name ${yacc_file} NAME_WE)
    set(copied_file "${build_dir}/${base_name}.mly")
    set(ocaml_file "${build_dir}/${base_name}.ml")
    set(interface_file "${build_dir}/${base_name}.mli")
    add_custom_command(
        OUTPUT ${ocaml_file} ${interface_file}
        COMMAND ${CMAKE_COMMAND} -E copy ${yacc_file} ${build_dir}
        COMMAND ${OCAML_YACC_EXECUTABLE} ${copied_file}
        DEPENDS ${yacc_file}
        WORKING_DIRECTORY ${build_dir}
        COMMENT "compiling OCaml Yacc file ${yacc_file}"
        VERBATIM)
    set(object_file "${build_dir}/${base_name}.cmo")
    set(interface_object_file "${build_dir}/${base_name}.cmi")
    add_ocaml_source(${object_file} ${ocaml_file}
        "${MY_DEPENDENCIES};grammar" SEPARATE_INTERFACE)
    add_ocaml_source(${interface_object_file} ${interface_file}
        "${MY_DEPENDENCIES}" SEPARATE_INTERFACE)
endfunction()

function(add_ocaml_lex_source lex_file)
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")
    cmake_parse_arguments(MY "" "" "DEPENDENCIES" ${ARGN})
    get_filename_component(base_name ${lex_file} NAME_WE)
    set(copied_file "${build_dir}/${base_name}.mll")
    set(ocaml_file "${build_dir}/${base_name}.ml")
    set(object_file "${build_dir}/${base_name}.cmo")
    add_custom_command(
        OUTPUT ${ocaml_file}
        COMMAND ${CMAKE_COMMAND} -E copy ${lex_file} ${build_dir}
        COMMAND ${OCAML_LEX_EXECUTABLE} ${copied_file}
        DEPENDS ${lex_file}
        WORKING_DIRECTORY ${build_dir}
        COMMENT "compiling OCaml Lex file ${lex_file}"
        VERBATIM)
    add_ocaml_source(${object_file} ${ocaml_file}
        "${MY_DEPENDENCIES}")
endfunction()

function(add_ocaml_executable executable)
    set(build_dir "${CMAKE_CURRENT_BINARY_DIR}")
    cmake_parse_arguments(MY "" "" "OBJECTS;LIBRARIES" ${ARGN})
    set(object_files)
    foreach(object ${MY_OBJECTS})
        list(APPEND object_files "${build_dir}/${object}.cmo")
    endforeach()
    set(library_files)
    foreach(library ${MY_LIBRARIES})
        list(APPEND library_files "${library}.cma")
    endforeach()
    if (WIN32)
        set(executable_file "${executable}.exe")
    else()
        set(executable_file ${executable})
    endif()
    add_custom_command(
        OUTPUT "${build_dir}/${executable_file}"
        COMMAND ${OCAML_EXECUTABLE} -o ${executable_file} ${library_files}
                ${object_files}
        DEPENDS ${object_files}
        WORKING_DIRECTORY ${build_dir}
        COMMENT "linking OCaml executable ${executable}"
        VERBATIM)
    add_custom_target(
        the_preprocessor ALL
        DEPENDS "${build_dir}/${executable_file}")
endfunction()

# preprocessor
set(preprocessor_dir "${CMAKE_CURRENT_SOURCE_DIR}")
add_regular_ocaml_source("${preprocessor_dir}/types.ml")
add_ocaml_lex_source("${preprocessor_dir}/lexer.mll"
    DEPENDENCIES types grammar)
add_ocaml_yacc_source("${preprocessor_dir}/grammar.mly"
    DEPENDENCIES types)
add_regular_ocaml_source("${preprocessor_dir}/utilities.ml"
    DEPENDENCIES types)
add_regular_ocaml_source("${preprocessor_dir}/functions.ml"
    DEPENDENCIES types utilities)
add_regular_ocaml_source("${preprocessor_dir}/records.ml"
    DEPENDENCIES types utilities)
add_regular_ocaml_source("${preprocessor_dir}/structures.ml"
    DEPENDENCIES types utilities functions)
add_regular_ocaml_source("${preprocessor_dir}/enums.ml"
    DEPENDENCIES types utilities functions)
add_regular_ocaml_source("${preprocessor_dir}/dependency.ml"
    DEPENDENCIES types utilities)
add_regular_ocaml_source("${preprocessor_dir}/provider.ml"
    DEPENDENCIES types utilities)
add_regular_ocaml_source("${preprocessor_dir}/mutations.ml"
    DEPENDENCIES types utilities)
add_regular_ocaml_source("${preprocessor_dir}/unions.ml"
    DEPENDENCIES types utilities enums functions)
add_regular_ocaml_source("${preprocessor_dir}/blocks.ml"
    DEPENDENCIES types utilities grammar lexer functions mutations)
add_regular_ocaml_source("${preprocessor_dir}/main.ml"
    DEPENDENCIES types utilities functions structures unions enums dependency
                 provider mutations records blocks)
add_ocaml_executable(preprocessor
    OBJECTS types utilities lexer grammar functions structures enums unions
            dependency provider mutations records blocks main
    LIBRARIES str)

# Record the path of the preprocessor executable that was built.
set(PREPROCESSOR_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/preprocessor")
if (WIN32)
    set(PREPROCESSOR_PATH "${PREPROCESSOR_PATH}.exe")
endif()
set(THE_PREPROCESSOR_PATH ${PREPROCESSOR_PATH} CACHE INTERNAL "")

# preprocess_header_files preprocesses a list of header files.
function(preprocess_header_files
    generated_cpp_files generated_header_files)

    set(zero_value_args )
    set(one_value_args ACCOUNT_ID TYPE_APP_ID FUNCTION_APP_ID NAMESPACE
        INDEX_FILE)
    set(multi_value_args INPUT_FILES)
    cmake_parse_arguments(MY "${zero_value_args}" "${one_value_args}"
        "${multi_value_args}" ${ARGN})

    set(generated_cpps)
    set(generated_headers)
    set(preprocessed_ids)

    foreach(preprocessed_file ${MY_INPUT_FILES})
        file(RELATIVE_PATH relative_path ${CMAKE_CURRENT_SOURCE_DIR}
            ${preprocessed_file})
        get_filename_component(subdir ${relative_path} PATH)
        get_filename_component(file_name ${preprocessed_file} NAME)
        get_filename_component(file_name_we ${preprocessed_file} NAME_WE)
        set(build_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${subdir}")
        file(MAKE_DIRECTORY ${build_dir})
        set(generated_cpp_file "${build_dir}/${file_name_we}.cpp")
        set(generated_header_file "${build_dir}/${file_name_we}.hpp")
        string(REGEX REPLACE "[:/\\\\\\.]" "_" file_id
            "${subdir}/${file_name_we}")
        add_custom_command(
            OUTPUT ${generated_cpp_file} ${generated_header_file}
            COMMAND ${THE_PREPROCESSOR_PATH} ${preprocessed_file}
                    ${generated_header_file} ${MY_ACCOUNT_ID}
                    ${MY_TYPE_APP_ID} ${MY_FUNCTION_APP_ID} ${file_id}
                    ${MY_NAMESPACE}
            DEPENDS the_preprocessor ${preprocessed_file}
            WORKING_DIRECTORY ${build_dir}
            COMMENT "preprocessing ${subdir}/${file_name}"
            VERBATIM)
        list(APPEND generated_headers ${generated_header_file})
        list(APPEND generated_cpps ${generated_cpp_file})
        list(APPEND preprocessed_ids ${file_id})
    endforeach()

    # Write the API index file.
    set(api_index
        "#include <cradle/core/api_types.hpp>\nnamespace ${MY_NAMESPACE} {\n")
    foreach(id ${preprocessed_ids})
        set(api_index
            "${api_index}void add_${id}_api(cradle::api_implementation& api)\;\n")
    endforeach()
    string(TOUPPER "${MY_NAMESPACE}" uppercase_namespace)
    set(api_index
        "${api_index}#define ${uppercase_namespace}_REGISTER_APIS(api)\\\n")
    foreach(id ${preprocessed_ids})
        set(api_index "${api_index}add_${id}_api(api)\;\\\n")
    endforeach()
    set(api_index "${api_index}\n}\n")
    file(WRITE "${MY_INDEX_FILE}" ${api_index})

    set(${generated_cpp_files} ${generated_cpps} PARENT_SCOPE)
    set(${generated_header_files} ${generated_headers} PARENT_SCOPE)

endfunction()
