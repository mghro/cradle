#include <cradle/fs/app_dirs.hpp>

#include <cradle/core/testing.hpp>
#include <cradle/fs/file_io.hpp>

using namespace cradle;

static void
reset_directory(file_path const& dir)
{
    if (exists(dir))
        remove_all(dir);
    create_directory(dir);
}

#ifdef WIN32

TEST_CASE("Windows app directories", "[fs][app_dirs]")
{
    // These are hard to test for several reasons: the results are dependent
    // on Windows version and user name, much of what's being tested is
    // related to inter-user permissions, their effects are impossible to
    // encapsulate within the test directory, etc.
    // So for now, I'm leaving these untested at the unit test level and
    // hoping that higher-level testing will be sufficient.
}

#else

TEST_CASE("XDG app directories", "[fs][app_dirs]")
{
    auto author = some(string("not_used_here"));
    auto app = string("cradle_xdg_test_case_app");

    // Keep everything we're doing local to the test directory.
    auto cwd = boost::filesystem::current_path();
    auto home_dir = cwd / "xdg_home";
    reset_directory(home_dir);

    // If none of the relevant environment variables are set, it should be an
    // error to get the app directories.
    set_environment_variable("HOME", "");
    set_environment_variable("XDG_CONFIG_HOME", "");
    set_environment_variable("XDG_CONFIG_DIRS", "");
    set_environment_variable("XDG_CACHE_HOME", "");
    REQUIRE_THROWS(get_user_config_dir(author, app));
    REQUIRE_THROWS(get_user_cache_dir(author, app));
    REQUIRE_THROWS(get_user_logs_dir(author, app));

    // If only HOME is set, the results should be based on that.
    set_environment_variable("HOME", home_dir.string());
    // config
    auto default_config_dir = home_dir / ".config" / app;
    REQUIRE(get_user_config_dir(author, app) == default_config_dir);
    REQUIRE(exists(default_config_dir));
    reset_directory(home_dir);
    // cache
    auto default_cache_dir = home_dir / ".cache" / app;
    REQUIRE(get_user_cache_dir(author, app) == default_cache_dir);
    REQUIRE(exists(default_cache_dir));
    reset_directory(home_dir);
    // logs
    auto default_logs_dir = home_dir / ".local" / "share" / app / "logs";
    REQUIRE(get_user_logs_dir(author, app) == default_logs_dir);
    REQUIRE(exists(default_logs_dir));
    reset_directory(home_dir);

    // If we try getting the config search path now, it should be empty
    // because the app directory doesn't exist.
    REQUIRE(get_config_search_path(author, app) == std::vector<file_path>());
    // If we create the user config directory, it should then be part of
    // the path.
    get_user_config_dir(author, app);
    REQUIRE(
        get_config_search_path(author, app)
        == std::vector<file_path>({get_user_config_dir(author, app)}));
    reset_directory(home_dir);

    // Check that relative paths aren't used.
    // config
    set_environment_variable("XDG_CONFIG_HOME", "abc/def");
    REQUIRE(get_user_config_dir(author, app) == default_config_dir);
    // cache
    set_environment_variable("XDG_CACHE_HOME", "abc/def");
    REQUIRE(get_user_cache_dir(author, app) == default_cache_dir);
    // data/logs
    set_environment_variable("XDG_DATA_HOME", "abc/def");
    REQUIRE(get_user_logs_dir(author, app) == default_logs_dir);

    // Set some custom directories and check that they're used.
    // config
    auto custom_config_dir = cwd / "xdg_config";
    reset_directory(custom_config_dir);
    set_environment_variable("XDG_CONFIG_HOME", custom_config_dir.string());
    REQUIRE(get_user_config_dir(author, app) == custom_config_dir / app);
    // cache
    auto custom_cache_dir = cwd / "xdg_cache";
    reset_directory(custom_cache_dir);
    set_environment_variable("XDG_CACHE_HOME", custom_cache_dir.string());
    REQUIRE(get_user_cache_dir(author, app) == custom_cache_dir / app);
    // data/logs
    auto custom_data_dir = cwd / "xdg_data";
    reset_directory(custom_data_dir);
    set_environment_variable("XDG_DATA_HOME", custom_data_dir.string());
    REQUIRE(get_user_logs_dir(author, app) == custom_data_dir / app / "logs");

    // Also add some config dirs (including some relative ones and some
    // without app dirs) and check that the search path is adjusted correctly.
    auto system_config_dir_a = cwd / "xdg_sys_config_a";
    reset_directory(system_config_dir_a);
    create_directory(system_config_dir_a / app);
    auto system_config_dir_b = cwd / "xdg_sys_config_b";
    reset_directory(system_config_dir_b);
    auto system_config_dir_c = cwd / "xdg_sys_config_c";
    reset_directory(system_config_dir_c);
    create_directory(system_config_dir_c / app);
    set_environment_variable(
        "XDG_CONFIG_DIRS",
        system_config_dir_b.string() + ":" + system_config_dir_a.string()
            + ":xdg_sys_config_c");
    REQUIRE(
        get_config_search_path(author, app)
        == std::vector<file_path>(
               {custom_config_dir / app, system_config_dir_a / app}));

    // This isn't really implemented, but check that it's doing the correct
    // fallback.
    REQUIRE(
        get_shared_cache_dir(author, app) == get_user_cache_dir(author, app));
}

#endif

TEST_CASE("search paths", "[fs][app_dirs]")
{
    auto cwd = boost::filesystem::current_path();
    auto search_dir = cwd / "search_paths";
    reset_directory(search_dir);
    create_directory(search_dir / "a");
    create_directory(search_dir / "b");
    create_directory(search_dir / "c");
    dump_string_to_file(search_dir / "a" / "foo.txt", "foo");
    dump_string_to_file(search_dir / "c" / "foo.txt", "foo");

    auto search_path = std::vector<file_path>({search_dir,
                                               search_dir / "b",
                                               search_dir / "d",
                                               search_dir / "c",
                                               search_dir / "a"});

    REQUIRE(
        search_in_path(search_path, "foo.txt") == search_dir / "c" / "foo.txt");
}
