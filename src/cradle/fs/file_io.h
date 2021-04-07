#ifndef CRADLE_FS_FILE_IO_H
#define CRADLE_FS_FILE_IO_H

#include <fstream>

#include <cradle/core/exception.h>
#include <cradle/fs/types.hpp>

namespace cradle {

// Open a file into the given fstream. Throw an error if the open operation
// fails, and enable the exception bits on the fstream so that subsequent
// failures will throw exceptions.
void
open_file(std::fstream& file, file_path const& path, std::ios::openmode mode);
void
open_file(std::ifstream& file, file_path const& path, std::ios::openmode mode);
void
open_file(std::ofstream& file, file_path const& path, std::ios::openmode mode);

// If the above fails, it throws the following exception.
CRADLE_DEFINE_EXCEPTION(open_file_error)
CRADLE_DEFINE_ERROR_INFO(file_path, file_path)
CRADLE_DEFINE_ERROR_INFO(std::ios::openmode, open_mode)

// Get the contents of a file as a string.
string
read_file_contents(file_path const& path);

// Write a string to a file (overwriting anything that might have been in it).
void
dump_string_to_file(file_path const& path, string const& contents);

} // namespace cradle

#endif
