cmake_minimum_required(VERSION 2.8.12)
project(cradle)

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

set(CMAKE_CXX_STANDARD 14)

include(cmake/version.cmake)

add_subdirectory(preprocessor)

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
        boost/1.67.0@conan/stable
        Catch/1.12.1@bincrafters/stable
        jsonformoderncpp/3.7.0@vthiery/stable
        msgpack/3.2.1@bincrafters/stable
        cotire/1.7.6@smspillaz/cotire
        libcurl/7.52.1@bincrafters/stable
        OpenSSL/1.0.2s@conan/stable
        sqlite3/3.21.0@bincrafters/stable
        FakeIt/2.0.4@gasuketsu/stable
        websocketpp/0.7.0@TyRoXx/stable
        zlib/1.2.11@conan/stable
        bzip2/1.0.6@conan/stable
        yaml-cpp/0.6.2@tmadden/stable
        spdlog/0.16.3@bincrafters/stable
    BASIC_SETUP
    BUILD missing
    OPTIONS
        boost:without_atomic=True
        boost:without_chrono=True
        boost:without_container=True
        boost:without_context=True
        boost:without_coroutine=True
        boost:without_graph=True
        boost:without_graph_parallel=True
        boost:without_log=True
        boost:without_math=True
        boost:without_mpi=True
        boost:without_serialization=True
        boost:without_signals=True
        boost:without_test=True
        boost:without_timer=True
        boost:without_type_erasure=True
        boost:without_wave=True
        FakeIt:integration=catch
        *:shared=False)

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
if(IS_GCC)
    add_compile_options(-Wall -Werror)
    # Disable warnings that are too strict.
    # Warnings about suggested parentheses occur naturally when using Catch.
    add_compile_options(-Wno-parentheses)
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
    # Also suppress linker warnings about missing .pdb files that seem to inevitably creep in.
    add_link_options(/ignore:4099)
endif()

# Enable "big objects" for Visual C++.
if(IS_MSVC)
    add_compile_options(/bigobj)
endif()

# Set some other compiler and linker flags to speed up builds.
if(IS_MSVC)
    add_exe_link_options(/Debug:FASTLINK)
endif()

# Exclude as much of the Windows API as possible to avoid #define pollution
# and speed up builds. Note that this has to be done via command-line arguments
# due to the use of precompiled hearders.
if(IS_MSVC)
    add_definitions(/DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX)
endif()

# Set build options for instrumenting test coverage.
if(IS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
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
set(preprocessed_files ${headers})
list(REMOVE_ITEM preprocessed_files
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/core/immutable.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/core/preprocessed.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/core/type_info.hpp")
preprocess_header_files(
    generated_srcs generated_headers
    ACCOUNT_ID mgh
    # TODO: Make this its own actual app...
    TYPE_APP_ID dosimetry
    FUNCTION_APP_ID dosimetry
    NAMESPACE cradle
    INDEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/api_index.hpp"
    INPUT_FILES ${preprocessed_files})

# Add the main library.
file(GLOB_RECURSE srcs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/third-party/*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/third-party/*.hpp")
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
    ${CMAKE_CURRENT_BINARY_DIR}/generated/src
    src
    src/third-party)
if(IS_GCC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(cradle PUBLIC --coverage)
endif()

# Add the server.
add_executable(server src/server.cpp)
target_link_libraries(server cradle)

# Add the unit test runner.
file(GLOB_RECURSE UNIT_TEST_FILES CONFIGURE_DEPENDS "tests/*.cpp")
add_executable(unit_test_runner ${UNIT_TEST_FILES})
target_link_libraries(unit_test_runner cradle)
target_include_directories(unit_test_runner
    PRIVATE ${PROJECT_SOURCE_DIR}/unit_tests)
# Exclude the test runner source file from Cotire because it acts as the "main"
# file for Catch and needs to include Catch specially.
set_source_files_properties(
    tests/runner.cpp
    PROPERTIES
        COTIRE_EXCLUDED ON)
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
