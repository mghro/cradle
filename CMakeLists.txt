cmake_minimum_required(VERSION 2.8.12)
project(cradle)

set(CMAKE_CXX_STANDARD 14)

# Set up fips.
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")
set(FIPS_EXCEPTIONS ON)
set(FIPS_RTTI ON)
fips_setup()
fips_project(cradle)

# Run Conan to install external C++ libraries.
# Conan and fips disagree on various build options, so we need to override
# some of Conan's defaults.
set(CONAN_OPTIONS)
if (FIPS_MSVC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CONAN_OPTIONS
            -s compiler=Visual\ Studio -s build_type=Debug -s compiler.runtime=MTd)
    else()
        set(CONAN_OPTIONS
            -s compiler=Visual\ Studio -s build_type=Release -s compiler.runtime=MT)
    endif()
elseif(FIPS_GCC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CONAN_OPTIONS
            -s compiler=gcc -s compiler.libcxx=libstdc++11 -s build_type=Debug -o Boost:fPIC=True)
    else()
        set(CONAN_OPTIONS
            -s compiler=gcc -s compiler.libcxx=libstdc++11 -s build_type=Release -o Boost:fPIC=True)
    endif()
endif()
execute_process(
    COMMAND conan install ${PROJECT_SOURCE_DIR} ${CONAN_OPTIONS} -e CONAN_IMPORT_PATH=${FIPS_PROJECT_DEPLOY_DIR} --build missing
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE CONAN_RESULT)
if(NOT ${CONAN_RESULT} EQUAL 0)
    message(FATAL_ERROR "Conan failed.")
endif()

# And now set up CMake to use those libraries.
# Note that Conan seems to insert flags that don't make sense and cause warnings.
set(ORIGINAL_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
include(${PROJECT_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()
set(CMAKE_CXX_FLAGS "${ORIGINAL_CXX_FLAGS}")

# Include cotire.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CONAN_COTIRE_ROOT}/cmake/cotire")
include(cotire)

# Add the given linker options on anything that gets linked.
macro(add_link_options )
    string(REPLACE ";" " " OPTIONS "${ARGV}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${OPTIONS}")
endmacro()

# Add the given linker options for executables.
macro(add_exe_link_options )
    string(REPLACE ";" " " OPTIONS "${ARGV}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIONS}")
endmacro()

# Enable a high level of compiler warnings and treat them as errors.
if(FIPS_GCC)
    add_compile_options(-Wall -Werror)
    # Disable warnings that are too strict.
    # Warnings about suggested parentheses occur naturally when using Catch.
    add_compile_options(-Wno-parentheses)
    # unused function parameters
    add_compile_options(-Wno-unused-function)
elseif(FIPS_MSVC)
    # First strip out the old warning level.
    string(REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    add_compile_options(/W4 /WX)
    # Disable warnings that are too strict.
    # "unreferenced formal parameter"
    add_compile_options(/wd4100)
    # "declaration hides previous local declaration"
    add_compile_options(/wd4456)
    # "unreferenced local function has been removed"
    add_compile_options(/wd4505)
    # warnings about functions that are potentially insecure
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    # Also suppress linker warnings about missing .pdb files that seem to inevitably creep in.
    add_link_options(/ignore:4099)
endif()

# Enable "big objects" for Visual C++.
if(FIPS_MSVC)
    add_compile_options(/bigobj)
endif()

# Set some other compiler and linker flags to speed up builds.
if(FIPS_MSVC)
    add_exe_link_options(/Debug:FASTLINK)
endif()

# Exclude as much of the Windows API as possible to avoid #define pollution
# and speed up builds. Note that this has to be done via command-line arguments
# due to the use of precompiled hearders.
if(FIPS_MSVC)
    add_definitions(/DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX)
endif()

# Set build options for instrumenting test coverage.
if(FIPS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling gcov support")
    add_compile_options(--coverage -fno-inline -fno-inline-small-functions -fno-default-inline -fprofile-arcs -ftest-coverage)
    add_exe_link_options(--coverage)
endif()

# Exclude from precompiled headers any local headers or overly specific libraries.
set_property(
    DIRECTORY
    PROPERTY
        COTIRE_PREFIX_HEADER_IGNORE_PATH
            ${PROJECT_SOURCE_DIR}/src
            ${CONAN_MSGPACK_ROOT}
            ${CONAN_CATCH_ROOT}
            ${CONAN_JSON_ROOT}
            ${CONAN_WEBSOCKETPP_ROOT}
            ${CONAN_SQLITE3_ROOT})

# Only generate tagged constructors on supported compilers.
if(FIPS_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "6.0")
    message(STATUS "Enabling tagged constructors")
    add_definitions("-DCRADLE_USING_TAGGED_CONSTRUCTORS")
    set(GENERATE_TAGGED_CONSTRUCTORS True)
else()
    set(GENERATE_TAGGED_CONSTRUCTORS False)
endif()

# Add the main library.
fips_begin_module(cradle)
    fips_src(src/cradle)
    fips_src(src/third-party)
    # There doesn't seem to be a better way to disable the warnings that are
    # generated by WebSocket++.
    if(FIPS_MSVC)
        set_property(
            SOURCE src/cradle/websocket/server.cpp
            APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
        set_property(
            SOURCE src/cradle/websocket/client.cpp
            APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
    endif()
    fips_dir(.)
    # Add rules for preprocessing YAML files.
    file(
        GLOB_RECURSE YAML_SOURCE_FILES
        RELATIVE ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/*.yml)
    foreach(FILE ${YAML_SOURCE_FILES})
        get_filename_component(FILE_DIRECTORY ${FILE} DIRECTORY)
        get_filename_component(FILE_NAME_WE ${FILE} NAME_WE)
        fips_generate(
            FROM ${PROJECT_SOURCE_DIR}/src/${FILE}
            TYPE cpp_api_generator
            OUT_OF_SOURCE
            SOURCE "generated/${FILE_DIRECTORY}/${FILE_NAME_WE}.cpp"
            HEADER "generated/${FILE_DIRECTORY}/${FILE_NAME_WE}.hpp"
            ARGS "{ app_name: cradle, generate_tagged_constructors: ${GENERATE_TAGGED_CONSTRUCTORS} }")
    endforeach()
    # Add the rule for generating version info.
    fips_generate(
        FROM ${PROJECT_SOURCE_DIR}/fips-generators/version_info_generator.py
        OUT_OF_SOURCE
        HEADER "generated/cradle/version_info.hpp")
fips_end_module()
target_link_libraries(cradle ${CONAN_LIBS})
target_link_directories(cradle PUBLIC ${CONAN_LIB_DIRS})
target_include_directories(cradle PUBLIC src)
target_include_directories(cradle PUBLIC src/third-party)
target_include_directories(cradle PUBLIC ${PROJECT_BINARY_DIR}/generated)
# Register Conan's include directories with fips.
target_include_directories(cradle PUBLIC ${CONAN_INCLUDE_DIRS}
    "$<$<CONFIG:Release>:${CONAN_INCLUDE_DIRS_RELEASE}>"
    "$<$<CONFIG:Debug>:${CONAN_INCLUDE_DIRS_DEBUG}>")
cotire(cradle)
if(FIPS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(cradle PUBLIC --coverage)
endif()

# Add the server.
fips_begin_app(server cmdline)
    fips_deps(cradle)
    fips_files(src/server.cpp)
fips_end_app()

# Add the test runner.
fips_begin_app(unit_test_runner cmdline)
    fips_deps(cradle)
    fips_src(tests)
    # Exclude the test runner source file from Cotire because it acts as the
    # "main" file for Catch and needs to include Catch specially.
    set_source_files_properties(
        tests/runner.cpp
        PROPERTIES
            COTIRE_EXCLUDED ON)
fips_end_app()
cotire(unit_test_runner)

# Add the unit testing target.
add_custom_target(
    unit_tests
    # Create a fresh 'testing' directory within the build dir and run the
    # tests with that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory testing
    COMMAND ${CMAKE_COMMAND} -E make_directory testing
    COMMAND ${CMAKE_COMMAND} -E chdir testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${FIPS_PROJECT_DEPLOY_DIR} ${FIPS_PROJECT_DEPLOY_DIR}/unit_test_runner
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS unit_test_runner)

# Add the unit test coverage target.
if(FIPS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_target(
        unit_test_coverage
        COMMAND lcov --directory . --zerocounters
        COMMAND ${CMAKE_COMMAND} --build . --target unit_tests
        COMMAND lcov --directory . --capture --output-file raw.info
        COMMAND lcov --extract raw.info '${PROJECT_SOURCE_DIR}/src/cradle/*' --output-file filtered.info
        COMMAND ${CMAKE_COMMAND} -E copy filtered.info ${PROJECT_SOURCE_DIR}/.lcov.info
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

# Add the integration testing target.
add_custom_target(
    integration_tests
    COMMAND ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${FIPS_PROJECT_DEPLOY_DIR} pytest
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/python
    DEPENDS server)

# Add a target for running all tests. (And include this as a default target.)
add_custom_target(
    all_tests ALL
    DEPENDS unit_tests
    DEPENDS integration_tests)

fips_finish()
