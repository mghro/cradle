#ifndef CRADLE_FS_APP_DIRS_HPP
#define CRADLE_FS_APP_DIRS_HPP

#include <cradle/fs/types.hpp>

// This file provides utilities for resolving directory locations according to
// the conventions of the OS.
//
// For Windows, it uses SHGetFolderPath to determine appropriate system-wide
// and user-specific directories for the app and then creates subdirectories
// in there for different purposes. (It is currently biased towards storing
// local data, so it uses the non-roaming user data directory.)
//
// For other systems, it uses the XDG standards.
//
// Note that there is currently no specific implementation for OS X.

namespace cradle {

// Get the directory that should be used to store user-specific configuration
// files. If the directory doesn't already exist, it is created.
file_path
get_user_config_dir(
    optional<string> const& author_name, string const& app_name);

// Get the full path of directories that should be searched for configuration
// files. (This may include system-wide configuration directories that are
// read-only to the user.)
//
// Note that since this is for read-only purposes, directories will only be
// returned if they already exist (specifically for this app). In particular,
// if you call this before calling get_user_config_dir() and the latter call
// ends up creating the user directory, you'll have to call this again if you
// want your search path to include that directory.
//
std::vector<file_path>
get_config_search_path(
    optional<string> const& author_name, string const& app_name);

// Get the directory that should be used for user-specific caching.
// If the directory doesn't already exist, it is created.
file_path
get_user_cache_dir(optional<string> const& author_name, string const& app_name);

// Get the directory that should be used for shared caching.
// If the directory doesn't already exist, it is created.
// Note that since some systems might not support this concept, this is
// allowed to return the user-specific cache directory.
file_path
get_shared_cache_dir(
    optional<string> const& author_name, string const& app_name);

// Given a search path and a relative path to a configuration file (or
// directory) that the application wants to read, this will scan the search
// path and return the full path to the first place it's found.
optional<file_path>
search_in_path(
    std::vector<file_path> const& search_path, file_path const& item);

// If one of the above is unable to create the requested directory, this
// exception is throw.
CRADLE_DEFINE_EXCEPTION(directory_creation_failure)
CRADLE_DEFINE_ERROR_INFO(file_path, directory_path)

} // namespace cradle

#endif
