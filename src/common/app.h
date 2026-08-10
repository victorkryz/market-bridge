#pragma once

#include "logs/logger.h"
#include "cxxopts.hpp"
#include "config.h"
#include "config/config_json.h"

constexpr auto app_name = APP_NAME;
constexpr auto app_version = APP_VERSION;
constexpr uint16_t default_http_port = DEFAULT_HTTP_PORT;
constexpr uint16_t default_https_port = DEFAULT_HTTPS_PORT;

void show_usage(const cxxopts::Options& options);
void merge_parsed_arguments(const cxxopts::ParseResult& parsed_cfg, Config& target_cfg);

// returns pair: error code, usage requested indicator
template <typename ConfigLoader = LoadFromFile>
inline std::pair<int, bool> process_arguments(int argc, char* argv[],
                                              ConfigLoader& cfg_loader, Config& cfg)
{
    std::pair<int, bool> result = {0, false};

    try
    {
        cxxopts::Options options(app_name, "Binance proxy server");
        options.positional_help("").show_positional_help();

        // clang-format off
        options.add_options()("p, http-port", "specify http port (default: 8080)",
                              cxxopts::value<decltype(cfg.server.http_port)>())

                              ("s, https-port", "specify port (default: 8443)",
                              cxxopts::value<decltype(cfg.server.https_port)>())

                              ("o, log-output", "specify logging output (file, console)",  
                              cxxopts::value<std::string>())

                              ("r, run-mode", "specify running mode (persist, single-request)",
                              cxxopts::value<std::string>())

                              ("l, log-level", "specify log level (error, warning, trace, debug, critical, off) (default: info)",
                              cxxopts::value<std::string>())

                              ("c, cert-path", "specify https server certificate path",
                              cxxopts::value<std::string>())

                              ("k, private-key", "specify https server private key path",
                              cxxopts::value<std::string>())

                              ("i, ignore-cert-verification", "ignore SSL certificate verification for outgoing requests",
                              cxxopts::value<bool>()->implicit_value("true"))

                              ("H, upstream-host", "specify upstream host for proxying outgoing requests",    
                              cxxopts::value<std::string>())

                              ("P, upstream-port", "specify upstream port for proxying outgoing requests (default: 443)",
                              cxxopts::value<decltype(cfg.upstream.port)>())

                              ("a, allow-https-over-http-port", "allow HTTPS requests over HTTP port",
                              cxxopts::value<bool>()->implicit_value("true"))

                              ("throttling-enabled", "enable request throttling",
                              cxxopts::value<bool>()->implicit_value("true"))

                              ("throttling-requests-per-second", "specify throttling requests per second (default: 20)",
                              cxxopts::value<decltype(cfg.throttling.requests_per_second)>())

                              ("throttling-burst-size", "specify throttling burst size (default: 40)",
                              cxxopts::value<decltype(cfg.throttling.burst_size)>())

                              ("config", "load configuration from a JSON file",
                              cxxopts::value<std::string>(cfg.config_path))

                              ("h, help", "print usage");
        // clang-format on

        auto parsed_cfg = options.parse(argc, argv);

        if (parsed_cfg.count("help"))
            result.second = true;
        else
        {
            if (parsed_cfg.count("config"))
            {
                cfg_loader.assign_path(cfg.config_path);
                load_json_config(cfg_loader, cfg);
            }

            merge_parsed_arguments(parsed_cfg, cfg);

            if (cfg.server.http_port == cfg.server.https_port)
            {
                std::cout << "http-port and https-port must be different" << std::endl;
                result.first = 1;
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

inline std::pair<int, bool> process_arguments(int argc, char* argv[], Config& cfg)
{
    LoadFromFile cfg_loader("");
    return process_arguments(argc, argv, cfg_loader, cfg);
}

inline void merge_parsed_arguments(const cxxopts::ParseResult& parsed_args, Config& target_cfg)
{
    if (parsed_args.count("http-port"))
        target_cfg.server.http_port = parsed_args["http-port"].as<decltype(target_cfg.server.http_port)>();

    if (parsed_args.count("https-port"))
        target_cfg.server.https_port = parsed_args["https-port"].as<decltype(target_cfg.server.https_port)>();

    if (parsed_args.count("cert-path"))
        target_cfg.tls.certificate = parsed_args["cert-path"].as<std::string>();

    if (parsed_args.count("private-key"))
        target_cfg.tls.private_key = parsed_args["private-key"].as<std::string>();

    if (parsed_args.count("ignore-cert-verification"))
        target_cfg.upstream.ignore_certificate_verification =
            parsed_args["ignore-cert-verification"].as<bool>();

    if (parsed_args.count("upstream-host"))
        target_cfg.upstream.host = parsed_args["upstream-host"].as<std::string>();

    if (parsed_args.count("upstream-port"))
        target_cfg.upstream.port = parsed_args["upstream-port"].as<decltype(target_cfg.upstream.port)>();

    if (parsed_args.count("allow-https-over-http-port"))
        target_cfg.server.allow_https_over_http_port =
            parsed_args["allow-https-over-http-port"].as<bool>();

    if (parsed_args.count("throttling-enabled"))
        target_cfg.throttling.enabled =
            parsed_args["throttling-enabled"].as<bool>();

    if (parsed_args.count("throttling-requests-per-second"))
        target_cfg.throttling.requests_per_second =
            parsed_args["throttling-requests-per-second"]
                .as<decltype(target_cfg.throttling.requests_per_second)>();

    if (parsed_args.count("throttling-burst-size"))
        target_cfg.throttling.burst_size =
            parsed_args["throttling-burst-size"]
                .as<decltype(target_cfg.throttling.burst_size)>();

    if (parsed_args.count("log-level"))
        target_cfg.logging.level = spdlog::level::from_str(
            parsed_args["log-level"].as<std::string>());

    if (parsed_args.count("log-output"))
    {
        const auto log_type = parsed_args["log-output"].as<std::string>();
        if ((log_type == "con") || (log_type == "console"))
            target_cfg.logging.output = LoggerType::Console;
        else if (log_type == "file")
            target_cfg.logging.output = LoggerType::File;
    }

    if (parsed_args.count("run-mode"))
    {
        const auto running_mode = parsed_args["run-mode"].as<std::string>();
        if (running_mode == "single-request")
            target_cfg.server.run_mode = ServerRunningMode::SingleRequest;
        else if (running_mode == "persist")
            target_cfg.server.run_mode = ServerRunningMode::Persistent;
    }
}
