#include <cradle/fs/app_dirs.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>

#ifdef WIN32

#include <AccCtrl.h>
#include <Aclapi.h>
#include <Shlobj.h>

#endif

namespace cradle {

#ifdef WIN32

static bool
create_directory_with_user_full_control_acl(string const& path)
{
    LPCTSTR lp_path = path.c_str();

    if (!CreateDirectory(lp_path, NULL))
        return false;

    HANDLE h_dir = CreateFile(
        lp_path,
        READ_CONTROL | WRITE_DAC,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
    if (h_dir == INVALID_HANDLE_VALUE)
        return false;

    ACL* p_old_dacl;
    SECURITY_DESCRIPTOR* p_sd = NULL;
    GetSecurityInfo(
        h_dir,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        NULL,
        NULL,
        &p_old_dacl,
        NULL,
        (void**) &p_sd);

    PSID p_sid = NULL;
    SID_IDENTIFIER_AUTHORITY auth_nt = SECURITY_NT_AUTHORITY;
    AllocateAndInitializeSid(
        &auth_nt,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_USERS,
        0,
        0,
        0,
        0,
        0,
        0,
        &p_sid);

    EXPLICIT_ACCESS ea = {0};
    ea.grfAccessMode = GRANT_ACCESS;
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
    ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = (LPTSTR) p_sid;

    ACL* p_new_dacl = 0;
    SetEntriesInAcl(1, &ea, p_old_dacl, &p_new_dacl);

    if (p_new_dacl)
    {
        SetSecurityInfo(
            h_dir,
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            NULL,
            NULL,
            p_new_dacl,
            NULL);
    }

    FreeSid(p_sid);
    LocalFree(p_new_dacl);
    LocalFree(p_sd);
    CloseHandle(h_dir);

    return true;
}

static void
create_directory_if_needed(file_path const& dir, bool shared = false)
{
    if (!exists(dir))
    {
        if (shared)
        {
            if (!create_directory_with_user_full_control_acl(dir.string()))
            {
                CRADLE_THROW(
                    directory_creation_failure() << directory_path_info(dir));
            }
        }
        else
        {
            create_directory(dir);
        }
    }
}

// Use SHGetFolderPath to get the path corresponding to :csidle_folder and
// return the directory within there that should be used by the given app.
// If :create is true, the directory will be created if it doesn't exist.
// If :shared is also true, the directory is created as a shared directory.
file_path
get_app_dir(
    int csidl_folder,
    optional<string> const& author_name,
    string const& app_name,
    bool create,
    bool shared = false)
{
    TCHAR path[MAX_PATH];
    if (create)
        csidl_folder |= CSIDL_FLAG_CREATE;
    if (SHGetFolderPath(NULL, csidl_folder, NULL, 0, path) != S_OK)
    {
        CRADLE_THROW(
            system_call_failed() << failed_system_call_info("SHGetFolderPath"));
    }
    file_path app_data_dir(path, boost::filesystem::native);
    file_path app_dir;
    if (author_name)
    {
        file_path author_dir = app_data_dir / *author_name;
        if (create)
            create_directory_if_needed(author_dir, shared);
        app_dir = author_dir / app_name;
    }
    else
    {
        app_dir = app_data_dir / app_name;
    }
    if (create)
        create_directory_if_needed(app_dir, shared);
    return app_dir;
}

file_path
get_user_config_dir(optional<string> const& author_name, string const& app_name)
{
    auto user_app_dir
        = get_app_dir(CSIDL_LOCAL_APPDATA, author_name, app_name, true);
    auto user_config_dir = user_app_dir / "config";
    create_directory_if_needed(user_config_dir);
    return user_config_dir;
}

std::vector<file_path>
get_config_search_path(
    optional<string> const& author_name, string const& app_name)
{
    std::vector<file_path> path;

    auto user_app_dir
        = get_app_dir(CSIDL_LOCAL_APPDATA, author_name, app_name, false);
    auto user_config_dir = user_app_dir / "config";
    if (exists(user_config_dir))
        path.push_back(user_config_dir);

    auto shared_app_dir
        = get_app_dir(CSIDL_COMMON_APPDATA, author_name, app_name, false);
    auto shared_config_dir = shared_app_dir / "config";
    if (exists(shared_config_dir))
        path.push_back(shared_config_dir);

    return path;
}

file_path
get_user_cache_dir(optional<string> const& author_name, string const& app_name)
{
    auto user_app_dir
        = get_app_dir(CSIDL_LOCAL_APPDATA, author_name, app_name, true);
    auto user_cache_dir = user_app_dir / "cache";
    create_directory_if_needed(user_cache_dir);
    return user_cache_dir;
}

file_path
get_user_logs_dir(optional<string> const& author_name, string const& app_name)
{
    auto user_app_dir
        = get_app_dir(CSIDL_LOCAL_APPDATA, author_name, app_name, true);
    auto user_logs_dir = user_app_dir / "logs";
    create_directory_if_needed(user_logs_dir);
    return user_logs_dir;
}

file_path
get_shared_cache_dir(
    optional<string> const& author_name, string const& app_name)
{
    auto shared_app_dir
        = get_app_dir(CSIDL_COMMON_APPDATA, author_name, app_name, true, true);
    auto shared_cache_dir = shared_app_dir / "cache";
    create_directory_if_needed(shared_cache_dir, true);
    return shared_cache_dir;
}

#else // Unix-based systems

static file_path
get_user_home_dir()
{
    return get_environment_variable("HOME");
}

static file_path
get_user_config_home()
{
    auto xdg_config_home = get_optional_environment_variable("XDG_CONFIG_HOME");
    if (xdg_config_home)
    {
        file_path dir = *xdg_config_home;
        // XDG requires absolute paths.
        if (dir.is_absolute())
            return dir;
    }
    return get_user_home_dir() / ".config";
}

file_path
get_user_config_dir(optional<string> const& author_name, string const& app_name)
{
    auto app_config_dir = get_user_config_home() / app_name;
    create_directories(app_config_dir);
    return app_config_dir;
}

std::vector<file_path>
get_config_search_path(
    optional<string> const& author_name, string const& app_name)
{
    std::vector<file_path> search_path;

    // Check for a user config dir.
    auto user_config_dir = get_user_config_home() / app_name;
    if (exists(user_config_dir))
        search_path.push_back(user_config_dir);

    // Get the list of XDG base config dirs.
    auto xdg_config_dirs = get_optional_environment_variable("XDG_CONFIG_DIRS");
    if (!xdg_config_dirs)
        xdg_config_dirs = "/etc/xdg";
    std::vector<string> dirs;
    boost::split(dirs, *xdg_config_dirs, [](char c) { return c == ':'; });

    // Filter for directories that contain a subdirectory for this app.
    for (auto const& dir : dirs)
    {
        file_path path(dir);
        if (path.is_absolute() && exists(path / app_name))
        {
            search_path.push_back(path / app_name);
        }
    }

    return search_path;
}

static file_path
get_user_cache_home()
{
    auto xdg_cache_home = get_optional_environment_variable("XDG_CACHE_HOME");
    if (xdg_cache_home)
    {
        file_path dir = *xdg_cache_home;
        // XDG requires absolute paths.
        if (dir.is_absolute())
            return dir;
    }
    return get_user_home_dir() / ".cache";
}

file_path
get_user_cache_dir(optional<string> const& author_name, string const& app_name)
{
    auto app_cache_dir = get_user_cache_home() / app_name;
    create_directories(app_cache_dir);
    return app_cache_dir;
}

static file_path
get_user_data_home()
{
    auto xdg_data_home = get_optional_environment_variable("XDG_DATA_HOME");
    if (xdg_data_home)
    {
        file_path dir = *xdg_data_home;
        // XDG requires absolute paths.
        if (dir.is_absolute())
            return dir;
    }
    return get_user_home_dir() / ".local" / "share";
}

file_path
get_user_logs_dir(optional<string> const& author_name, string const& app_name)
{
    auto app_logs_dir = get_user_data_home() / app_name / "logs";
    create_directories(app_logs_dir);
    return app_logs_dir;
}

file_path
get_shared_cache_dir(
    optional<string> const& author_name, string const& app_name)
{
    return get_user_cache_dir(author_name, app_name);
}

#endif

optional<file_path>
search_in_path(std::vector<file_path> const& search_path, file_path const& item)
{
    for (auto const& dir : search_path)
    {
        auto full_path = dir / item;
        if (exists(full_path))
            return some(full_path);
    }
    return none;
}

} // namespace cradle
