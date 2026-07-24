#pragma once

#include <cstdint>
#include <string>

#include "logs/logger.h"


enum class ServerRunningMode
{
    Persistent,   // Default: handle multiple requests
    SingleRequest // Handle exactly one request, then stop
};

struct Config
{
    struct Server
    {
        uint16_t http_port = DEFAULT_HTTP_PORT;
        uint16_t https_port = DEFAULT_HTTPS_PORT;
        ServerRunningMode run_mode = ServerRunningMode::Persistent;
        bool allow_https_over_http_port = false;
        uint16_t worker_threads = 3;
    } server;

    struct TLS
    {
        std::string certificate = "cert/server.crt";
        std::string private_key = "cert/server.key";
    } tls;

    struct Upstream
    {
        std::string host = "api.binance.com";
        uint16_t port = 443;
        bool ignore_certificate_verification = false;
    } upstream;

    struct Logging
    {
        LoggerType output = LoggerType::Console;
        spdlog::level::level_enum level = spdlog::level::level_enum::info;
    } logging;

    std::string config_path;
};
