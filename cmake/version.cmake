# Run 'git describe' and capture its output.
execute_process(COMMAND git describe --tags --dirty --long
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE git_description)
# Strip trailing newline.
string(REPLACE "\n" "" git_description "${git_description}")
# Split the output into its fragments.
# The description is in the form "<tag>-<commits-since-tag>-<hash>[-dirty]".
# (Note that <tag> may have internal '-' characters'.)
string(REPLACE "-" ";" fragments "${git_description}")
# Even if the tag doesn't have any internal dashes and there is no dirty
# component, we should still have three fragments in the description.
list(LENGTH fragments fragment_count)
if(${fragment_count} LESS 3)
    message(FATAL_ERROR "unable to parse 'git describe' output")
endif()

# Now work backwards interpreting the parts...

# Check for the dirty flag.
set(is_dirty FALSE)
math(EXPR index "${fragment_count} - 1")
list(GET fragments ${index} tail)
if (tail STREQUAL "dirty")
    set(is_dirty TRUE)
    math(EXPR index "${index} - 1")
endif()
string(TOLOWER ${is_dirty} is_dirty)

# Get the commit hash.
list(GET fragments ${index} commit_hash)
math(EXPR index "${index} - 1")

# Get the commits since the tag.
list(GET fragments ${index} commits_since_tag)

# The rest should be the tag.
list(SUBLIST fragments 0 ${index} tag)
string(JOIN "-" tag "${tag}")

# Generate the C++ code to represent all this.
set(cpp_code "\
// AUTOMATICALLY GENERATED!! - See version.cmake.\n\
static cradle::repository_info const version_info{\n\
  \"${commit_hash}\", ${is_dirty}, \"${tag}\", ${commits_since_tag} };\n\
")

# Generate the header file.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/")
set(header_file
    "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/version_info.hpp")
file(GENERATE OUTPUT "${header_file}" CONTENT "${cpp_code}")
