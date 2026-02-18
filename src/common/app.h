#pragma once

constexpr auto app_name = APP_NAME;
constexpr uint16_t default_http_port = DEFAULT_HTTP_PORT;

struct ClArguments
{
    uint16_t port = 8080; // default port
    ServerRunningMode running_mode = ServerRunningMode::Persistent;
    spdlog::level::level_enum log_level = spdlog::level::level_enum::info; // default log level
    LoggerType logger_type = LoggerType::File;
};

void show_usage(const cxxopts::Options& options);

// returns pair: error code, usage requested indicator
inline std::pair<int, bool> process_arguments(int argc, char* argv[], ClArguments& args)
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
                if ((log_type == "con") || (log_type == "console"))
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
