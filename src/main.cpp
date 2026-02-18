
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cxxopts.hpp>

#include "logs/logger.h"
#include "server.h"
#include "common/app.h"

namespace fsys = std::filesystem;

std::shared_ptr<spdlog::logger> init_logger(LoggerType, spdlog::level::level_enum logging_level);

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
