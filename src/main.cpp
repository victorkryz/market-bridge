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
    uint16_t port = 8080; // default port
    ServerRunningMode running_mode = ServerRunningMode::Persistent;
    spdlog::level::level_enum log_level = spdlog::level::level_enum::info; // default log level
    LoggerType logger_type = LoggerType::File;
};

std::pair<int, bool> process_arguments(int argc, char* argv[], ClArguments& args);
void show_usage(const cxxopts::Options& options);

int main(int argc, char* argv[])
{
    int result(0);
    try
    {
        ClArguments args;

        if (argc > 1)
        {
            const auto [exit_code, usage_requested] =
                process_arguments(argc, argv, args);
            if (exit_code != 0)
                return exit_code;
            else if (usage_requested)
                return 0;
        }

        gl_logger = init_logger(args.logger_type, args.log_level);

        Server server(args.port, args.running_mode);
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        result = 1;
    }
    return result;
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

        logger = spdlog::daily_logger_mt("logger_1", relative_log_file_path);
    }
    else
    {
        logger = spdlog::stdout_color_mt(app_name);
    }

    spdlog::set_pattern("[%H:%M:%S] %v");
    logger->set_level(logging_level);
    logger->flush_on(logging_level);

    return logger;
}

// returns pair: error code, usage requested indicator
std::pair<int, bool> process_arguments(int argc, char* argv[], ClArguments& args)
{
    bool usage(false), version(false);
    std::pair<int, bool> result = {0, false};

    try
    {
        cxxopts::Options options(app_name, "Binance proxy server");
        options.positional_help("[optional args]").show_positional_help();

        std::string log_level(SPDLOG_LEVEL_NAME_INFO.data(), SPDLOG_LEVEL_NAME_INFO.size());
        std::string running_mode, log_type;

        // clang-format off
        options.add_options()("p, port", "specify port (default: 8080)",
                              cxxopts::value<decltype(args.port)>(args.port))
                              ("t, log_type", "specify logging type (file, con)",
                              cxxopts::value<std::string>(log_type)->default_value("file"))
                               ("r, run_mode", "specify running mode (persist, single_request)",
                              cxxopts::value<std::string>(running_mode)->default_value("persist"))
                              ("l, log_level", "specify log level (error, warning, trace, debug, critical, off) (default: info)",
                              cxxopts::value<std::string>(log_level))
                              ("h, help", "print usage");
        // clang-format on

        auto parsed_args = options.parse(argc, argv);

        if (parsed_args.count("help"))
            result.second = true;
        else
        {
            if (!log_level.empty())
                args.log_level = spdlog::level::from_str(log_level);
            if (!log_type.empty())
            {
                if (log_type == "con")
                    args.logger_type = LoggerType::Console;
            }
            if (!running_mode.empty())
            {
                if (running_mode == "single_request")
                    args.running_mode = ServerRunningMode::SingleRequest;
            }
        }

        if (result.second)
            show_usage(options);
    }
    catch (const cxxopts::exceptions::exception& e)
    {
        std::cout << "command line arguments parsing error: " << e.what() << std::endl;
        result.first = 1;
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
    std::cout << std::endl
              << help << std::endl;
    std::cout << samples_of_using << std::endl
              << std::endl;
}
