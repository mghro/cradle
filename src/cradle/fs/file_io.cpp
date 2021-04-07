#include <cradle/fs/file_io.h>

#include <cradle/utilities/errors.h>

namespace cradle {

void
open_file(std::fstream& file, file_path const& path, std::ios::openmode mode)
{
    file.open(path.c_str(), mode);
    if (!file)
    {
        CRADLE_THROW(
            open_file_error() << file_path_info(path) << open_mode_info(mode)
                              << internal_error_message_info(strerror(errno)));
    }
    file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
}
void
open_file(std::ifstream& file, file_path const& path, std::ios::openmode mode)
{
    file.open(path.c_str(), mode);
    if (!file)
    {
        CRADLE_THROW(
            open_file_error() << file_path_info(path) << open_mode_info(mode)
                              << internal_error_message_info(strerror(errno)));
    }
    file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
}
void
open_file(std::ofstream& file, file_path const& path, std::ios::openmode mode)
{
    file.open(path.c_str(), mode);
    if (!file)
    {
        CRADLE_THROW(
            open_file_error() << file_path_info(path) << open_mode_info(mode)
                              << internal_error_message_info(strerror(errno)));
    }
    file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
}

string
read_file_contents(file_path const& path)
{
    std::ifstream in;
    open_file(in, path, std::ios::in | std::ios::binary);
    string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return contents;
}

void
dump_string_to_file(file_path const& path, string const& contents)
{
    std::ofstream output;
    open_file(
        output, path, std::ios::out | std::ios::trunc | std::ios::binary);
    output << contents;
}

} // namespace cradle
