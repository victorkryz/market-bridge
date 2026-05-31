#pragma once

#include "logs/logger.h"
#include "cxxopts.hpp"
#include "config.h"

constexpr auto app_name = APP_NAME;
constexpr auto app_version = APP_VERSION;
constexpr uint16_t default_http_port = DEFAULT_HTTP_PORT;
constexpr uint16_t default_https_port = DEFAULT_HTTPS_PORT;

void show_usage(const cxxopts::Options& options);

// returns pair: error code, usage requested indicator
inline std::pair<int, bool> process_arguments(int argc, char* argv[], Config& cfg)
{
    bool usage(false), version(false);
    std::pair<int, bool> result = {0, false};

    try
    {
        cxxopts::Options options(app_name, "Binance proxy server");
        options.positional_help("[optional cfg]").show_positional_help();

        std::string log_level(SPDLOG_LEVEL_NAME_INFO.data(), SPDLOG_LEVEL_NAME_INFO.size());
        std::string running_mode, log_type;

        // clang-format off
        options.add_options()("p, http-port", "specify http port (default: 8080)",
                              cxxopts::value<decltype(cfg.http_port)>(cfg.http_port))

                              ("s, https-port", "specify port (default: 8443)",
                              cxxopts::value<decltype(cfg.https_port)>(cfg.https_port))  

                              ("o, log-output", "specify logging output (file, console)",  
                              cxxopts::value<std::string>(log_type)->default_value("console"))

                              ("r, run-mode", "specify running mode (persist, single-request)",
                              cxxopts::value<std::string>(running_mode)->default_value("persist"))

                              ("l, log-level", "specify log level (error, warning, trace, debug, critical, off) (default: info)",
                              cxxopts::value<std::string>(log_level))

                              ("c, cert-path", "specify https server certificate path",
                              cxxopts::value<std::string>(cfg.srv_cert_path)->default_value("cert/server.crt"))

                              ("k, private-key", "specify https server private key path",
                              cxxopts::value<std::string>(cfg.srv_private_key_path)->default_value("cert/server.key"))

                              ("i, ignore-cert-verification", "ignore SSL certificate verification for outgoing requests",
                              cxxopts::value<bool>(cfg.ignore_certificate_verification)->default_value("false"))

                              ("H, upstream-host", "specify upstream host for proxying outgoing requests",    
                              cxxopts::value<std::string>(cfg.upstream_host)->default_value(cfg.upstream_host))

                              ("P, upstream-port", "specify upstream port for proxying outgoing requests (default: 443)",
                              cxxopts::value<decltype(cfg.upstream_port)>(cfg.upstream_port))

                              ("h, help", "print usage");
        // clang-format on

        auto parsed_cfg = options.parse(argc, argv);

        if (parsed_cfg.count("help"))
            result.second = true;
        else
        {
            if (!log_level.empty())
                cfg.log_level = spdlog::level::from_str(log_level);
            if (!log_type.empty())
            {
                if ((log_type == "con") || (log_type == "console"))
                    cfg.logger_type = LoggerType::Console;
                else if (log_type == "file")
                    cfg.logger_type = LoggerType::File;
            }
            if (!running_mode.empty())
            {
                if (running_mode == "single-request")
                    cfg.running_mode = ServerRunningMode::SingleRequest;
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
