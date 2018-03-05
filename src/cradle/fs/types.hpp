#ifndef CRADLE_FS_TYPES_HPP
#define CRADLE_FS_TYPES_HPP

#include <boost/filesystem/path.hpp>
#include <cradle/common.hpp>

namespace cradle {

// Use Boost.FileSystem's path as our official type of representing paths.
// (Note that file_path is of course slightly incorrect because the path could
// refer to a directory, but it's a lot easier to read and this seems like a
// pretty common usage (e.g., Chromium does it too).)
typedef boost::filesystem::path file_path;

} // namespace cradle

#endif
