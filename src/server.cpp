#include <cradle/websocket/server.hpp>

#include <boost/program_options.hpp>

#include <cradle/encodings/json.hpp>
#include <cradle/fs/app_dirs.hpp>
#include <cradle/fs/file_io.hpp>
#include <cradle/version_info.hpp>

using namespace cradle;

void static
show_version_info()
{
    if (is_tagged_version(version_info))
    {
        std::cout << "CRADLE " << version_info.tag << "\n";
    }
    else
    {
        std::cout <<
            "CRADLE (unreleased version - " <<
            version_info.commit_object_name << ", " <<
            version_info.commits_since_tag << " commits ahead of " << version_info.tag;
        if (version_info.dirty)
            std::cout << ", with local modifications";
        std::cout << ")\n";
    }
}

int
main(int argc, char const* const* argv)
{
    namespace po = boost::program_options;

    po::options_description desc("Supported options");
    desc.add_options()
        ("help", "show help message")
        ("version", "show version information")
        ("config-file", po::value<string>(), "specify the configuration file to use")
    ;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        show_version_info();
        std::cout << desc;
        return 0;
    }

    if (vm.count("version"))
    {
        show_version_info();
        return 0;
    }

    optional<file_path> config_path;
    if (vm.count("config-file"))
    {
        config_path = vm["config-file"].as<string>();
    }
    else
    {
        config_path = search_in_path(get_config_search_path(none, "cradle"), "config.json");
    }

    server_config config;
    if (config_path)
        from_dynamic(&config, parse_json_value(read_file_contents(*config_path)));

    websocket_server server(config);
    server.listen();
    server.run();
    return 0;
}
