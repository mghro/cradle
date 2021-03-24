#include <cradle/fs/file_io.hpp>

#include <cradle/core/testing.hpp>

#include <boost/filesystem/operations.hpp>

using namespace cradle;

template<class File>
void
test_bad_open_file(std::ios::openmode mode)
{
    file_path path("/very/likely/to-be/bad/file/path/asfqwfa/--test");

    File file;
    try
    {
        open_file(file, path, mode);
        FAIL("no exception thrown");
    }
    catch (open_file_error& e)
    {
        REQUIRE(get_required_error_info<file_path_info>(e) == path);
        REQUIRE(get_required_error_info<open_mode_info>(e) == mode);
        get_required_error_info<internal_error_message_info>(e);
    }
}

TEST_CASE("file open errors", "[fs][file_io]")
{
    test_bad_open_file<std::fstream>(
        std::ios::binary | std::ios::out | std::ios::trunc);
    test_bad_open_file<std::ifstream>(std::ios::in);
    test_bad_open_file<std::ofstream>(
        std::ios::binary | std::ios::out | std::ios::trunc);
}

TEST_CASE("file error bits set", "[fs][file_io]")
{
    {
        std::fstream fs;
        open_file(
            fs, file_path("empty_file.txt"), std::ios::out | std::ios::trunc);
        int i;
        REQUIRE_THROWS(fs >> i);
    }
}

TEST_CASE("read_file_contents", "[fs][file_io]")
{
    auto path = file_path("read_file_contents.txt");
    if (exists(path))
        remove(path);
    auto text = "some simple\n  text\n";
    dump_string_to_file(path, text);
    REQUIRE(read_file_contents(path) == text);
}
