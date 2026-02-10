#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <spdlog/common.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "logs/logger.h"
#include "server.h"

namespace fsys = std::filesystem;

constexpr auto app_name = APP_NAME;

std::shared_ptr<spdlog::logger> init_logger(LoggerType, spdlog::level::level_enum logging_level);

struct ClArguments
{
    uint16_t port = 8080;                                                  // default port
    spdlog::level::level_enum log_level = spdlog::level::level_enum::info; // default log level
};

bool process_arguments(int argc, char* argv[], ClArguments& args);
void show_usage(const cxxopts::Options& options);

int main(int argc, char* argv[])
{
    try
    {
        ClArguments args;

        if (argc > 1 && !process_arguments(argc, argv, args))
            return 1;

        gl_logger = init_logger(LoggerType::File, args.log_level);

        Server server(args.port);
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger> init_logger(LoggerType logger_type,
                                            spdlog::level::level_enum logging_level)
{
    using namespace std::literals;

    constexpr const auto sub_folder_name("logs"sv);
    constexpr const auto log_file_name("mb-log.log"sv);

    std::shared_ptr<spdlog::logger> logger;

    if (logger_type == LoggerType::File)
    {
        if (!fsys::exists(sub_folder_name))
            fsys::create_directory(sub_folder_name);

        fsys::path log_sub_folder(sub_folder_name);
        fsys::path relative_log_file_path = log_sub_folder / fsys::path(log_file_name);

        logger = spdlog::daily_logger_st(app_name, relative_log_file_path);
    }
    else
    {
        logger = spdlog::stdout_color_mt(app_name);
    }

    spdlog::set_pattern("[%H:%M:%S] %v");
    logger->set_level(logging_level);

    return logger;
}

bool process_arguments(int argc, char* argv[], ClArguments& args)
{
    bool result(false), usage(false), version(false);

    try
    {
        cxxopts::Options options(app_name, "binance proxy server");
        options.positional_help("[optional args]").show_positional_help();

        std::string log_level(SPDLOG_LEVEL_NAME_INFO.data(), SPDLOG_LEVEL_NAME_INFO.size());

        options.add_options()("p, port", "specify port (default 8080)",
                              cxxopts::value<decltype(args.port)>(args.port))(
            "l, log_level", "specify log level (error, warning, trace, debug, critical, off)",
            cxxopts::value<std::string>(log_level))("h, help", "print usage");

        auto parsed_args = options.parse(argc, argv);

        if (parsed_args.count("help"))
        {
            usage = true;
            result = true;
        }
        else
        {
            result = true;
            if (!log_level.empty())
            {
                result = true;
                args.log_level = spdlog::level::from_str(log_level);
            }
        }

        if (usage || !result)
            show_usage(options);
    }
    catch (const cxxopts::exceptions::exception& e)
    {
        std::cout << "command line arguments parsing error: " << e.what() << std::endl;
        result = false;
    }

    return result;
}

void show_usage(const cxxopts::Options& options)
{
    const char* samples_of_using_templ =
        R"(Command line samples:
    {app_name}
    {app_name} -p 8080
    {app_name} -l info)";

    std::string samples_of_using =
        fmt::format(samples_of_using_templ, fmt::arg("app_name", app_name));

    std::string help = options.help();
    std::cout << std::endl << help << std::endl;
    std::cout << samples_of_using << std::endl << std::endl;
}
