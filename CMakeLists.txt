cmake_minimum_required(VERSION 3.14)
project(cradle)

# Define the options for using the preprocessor.
# TODO: Add a Docker option.
set(PREPROCESSOR_USAGE_OPTIONS Native External)
set(PREPROCESSOR_USAGE Native CACHE STRING
    "How the preprocessor should be invoked")
set_property(CACHE PREPROCESSOR_USAGE PROPERTY
    STRINGS ${PREPROCESSOR_USAGE_OPTIONS})
set(EXTERNALLY_PREPROCESSED_DIRECTORY "" CACHE STRING
    "If PREPROCESSOR_USAGE is External, this is the directory that contains the externally preprocessed files.")

# Define the option for local Docker testing.
set(LOCAL_DOCKER_TESTING ON CACHE BOOL
    "Whether or not to enable testing that requires a local Docker instance")
if(LOCAL_DOCKER_TESTING)
    add_compile_options(-DLOCAL_DOCKER_TESTING)
endif()

# Detect the compiler.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(IS_CLANG true)
else()
    set(IS_CLANG false)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(IS_GCC true)
else()
    set(IS_GCC false)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(IS_MSVC true)
else()
    set(IS_MSVC false)
endif()

set(CMAKE_CXX_STANDARD 17)

include(cmake/version.cmake)

# Download/include CMake/Conan integration.
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
                  "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake"
                  TLS_VERIFY ON)
endif()
include(${CMAKE_CURRENT_BINARY_DIR}/conan.cmake)

# Invoke Conan.
conan_add_remote(
    NAME bincrafters
    INDEX 1
    URL https://api.bintray.com/conan/bincrafters/public-conan
    VERIFY_SSL True)
conan_cmake_run(
    REQUIRES
        boost/1.75.0
        catch2/2.13.4
        nlohmann_json/3.9.1
        msgpack/3.3.0
        libcurl/7.75.0
        openssl/1.1.1j
        sqlite3/3.35.2
        fakeit/2.0.7
        websocketpp/0.8.2
        zlib/1.2.11
        bzip2/1.0.8
        yaml-cpp/0.6.3
        spdlog/1.8.2
        simdjson/0.9.1
    BASIC_SETUP
    BUILD missing
    OPTIONS
        boost:without_chrono=True
        boost:without_container=True
        boost:without_context=True
        boost:without_contract=True
        boost:without_coroutine=True
        boost:without_fiber=True
        boost:without_filesystem=True
        boost:without_graph=True
        boost:without_graph_parallel=True
        boost:without_iostreams=True
        boost:without_json=True
        boost:without_locale=True
        boost:without_log=True
        boost:without_mpi=True
        boost:without_nowide=True
        boost:without_serialization=True
        boost:without_test=True
        boost:without_thread=True
        boost:without_timer=True
        boost:without_type_erasure=True
        boost:without_wave=True
        fakeit:integration=catch
        *:shared=False)

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
if(IS_GCC)
    add_compile_options(-Wall -Werror)
    # Disable warnings that are too strict.
    # unused function parameters
    add_compile_options(-Wno-unused-parameter)
    # unused functions
    add_compile_options(-Wno-unused-function)
elseif(IS_MSVC)
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
    add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    # Also suppress linker warnings about missing .pdb files that seem to inevitably creep in.
    add_link_options(/ignore:4099)
endif()

if(IS_MSVC)
    # Enable "big objects" for Visual C++.
    add_compile_options(/bigobj)
    # Set some other compiler and linker flags to speed up builds.
    add_exe_link_options(/Debug:FASTLINK)
    # Use absolute paths for error messages.
    add_compile_options(/FC)
endif()

# Exclude as much of the Windows API as possible to avoid #define pollution
# and speed up builds. Note that this has to be done via command-line arguments
# due to the use of precompiled hearders.
if(IS_MSVC)
    add_definitions(/DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX)
endif()

# Set build options for instrumenting test coverage.
enable_testing()
if(IS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling gcov support")
    add_compile_options(--coverage -fno-inline -fno-inline-small-functions -fno-default-inline -fprofile-arcs -ftest-coverage)
    add_exe_link_options(--coverage)
endif()

# Only generate tagged constructors on supported compilers.
if(IS_GCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "6.0")
    message(STATUS "Enabling tagged constructors")
    add_definitions("-DCRADLE_USING_TAGGED_CONSTRUCTORS")
    set(GENERATE_TAGGED_CONSTRUCTORS True)
else()
    set(GENERATE_TAGGED_CONSTRUCTORS False)
endif()

# Preprocess the API headers.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
if("${PREPROCESSOR_USAGE}" STREQUAL "Native")
    message(STATUS "Using native preprocessor")
    add_subdirectory(preprocessor)
    set(preprocessed_files ${headers})
    preprocess_header_files(
        generated_srcs generated_headers
        ACCOUNT_ID mgh
        # TODO: Make this its own actual app...
        TYPE_APP_ID dosimetry
        FUNCTION_APP_ID dosimetry
        NAMESPACE cradle
        INDEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/api_index.hpp"
        INPUT_FILES ${preprocessed_files})
    add_custom_target(preprocessing
        DEPENDS ${generated_srcs} ${generated_headers})
    set(generated_include_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
elseif("${PREPROCESSOR_USAGE}" STREQUAL "External")
    message(STATUS "Using externally preprocessed files in:")
    message(STATUS "  ${EXTERNALLY_PREPROCESSED_DIRECTORY}")
    get_filename_component(
        resolved_preprocessed_directory
        "${EXTERNALLY_PREPROCESSED_DIRECTORY}"
        ABSOLUTE)
    message(STATUS "  (${resolved_preprocessed_directory})")
    file(GLOB_RECURSE generated_srcs
        "${resolved_preprocessed_directory}/*.cpp")
    file(GLOB_RECURSE generated_headers
        "${resolved_preprocessed_directory}/*.hpp")
    set(generated_include_dir ${resolved_preprocessed_directory})
else()
    message(FATAL_ERROR "Invalid setting for PREPROCESSOR_USAGE")
endif()

# Add the main library.
file(GLOB_RECURSE srcs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(cradle STATIC ${srcs} ${headers})
# There doesn't seem to be a better way to disable the warnings that are
# generated by WebSocket++.
if(IS_MSVC)
    set_property(
        SOURCE src/cradle/websocket/server.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
    set_property(
        SOURCE src/cradle/websocket/client.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
endif()
target_sources(cradle PRIVATE ${generated_headers} ${generated_srcs}
    "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/version_info.hpp")
target_link_libraries(cradle ${CONAN_LIBS})
target_link_directories(cradle PUBLIC ${CONAN_LIB_DIRS})
target_compile_definitions(
    cradle PUBLIC ${CONAN_DEFINES} ${CONAN_COMPILE_DEFINITIONS})
target_include_directories(cradle PUBLIC
    ${CONAN_INCLUDE_DIRS}
    ${CLIPPER_INCLUDE_DIR}
    ${generated_include_dir}/src
    src
    src/third-party)
if(IS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(cradle PUBLIC --coverage)
endif()

# Set up the deployment directory.
set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}/deploy")
file(MAKE_DIRECTORY ${deploy_dir})

# Add the server.
add_executable(server src/server.cpp)
target_link_libraries(server cradle)
add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:server> ${deploy_dir})

# Add the unit test runner.
file(GLOB_RECURSE UNIT_TEST_FILES CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(unit_test_runner ${UNIT_TEST_FILES})
target_link_libraries(unit_test_runner cradle)
target_include_directories(unit_test_runner
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests)
# Don't use precompiled headers for the test runner since it needs to include
# Catch specially.
set_source_files_properties(
    tests/runner.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON)

# Retrieve the token info if it exists
if (DEFINED ENV{CRADLE_THINKNODE_API_TOKEN})
    set(API_TOKEN $ENV{CRADLE_THINKNODE_API_TOKEN})
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.token")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.token" API_TOKEN)
else()
    message(WARNING "Couldn't find API token.")
    set(API_TOKEN "(missing)")
endif()
string(REGEX REPLACE "\n" "" API_TOKEN "${API_TOKEN}")

# Add the unit testing target.
add_custom_target(
    unit_tests_target
    # Create a fresh 'testing' directory within the build dir and run the
    # tests with that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory testing
    COMMAND ${CMAKE_COMMAND} -E make_directory testing
    COMMAND ${CMAKE_COMMAND} -E chdir testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} $<TARGET_FILE:unit_test_runner>
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS unit_test_runner)
add_test(
    NAME unit_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target unit_tests_target)

# Add the unit test coverage target.
if(IS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_target(
        unit_test_coverage_target
        COMMAND lcov --directory . --zerocounters
        COMMAND ${CMAKE_COMMAND} --build . --target unit_tests
        COMMAND lcov --directory . --capture --output-file raw.info
        COMMAND lcov --extract raw.info '${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/*' --output-file filtered.info
        COMMAND ${CMAKE_COMMAND} -E copy filtered.info ${CMAKE_CURRENT_SOURCE_DIR}/.lcov.info
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
    add_test(
        NAME unit_test_coverage
        COMMAND ${CMAKE_COMMAND}
            --build ${CMAKE_BINARY_DIR}
            --target unit_test_coverage_target)
endif()

# Add the integration testing target.
add_custom_target(
    integration_tests_target
    COMMAND ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} pytest
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
    DEPENDS server)
add_test(
    NAME integration_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target integration_tests_target)
