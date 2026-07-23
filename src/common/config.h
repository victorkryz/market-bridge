#pragma once

#include "logs/logger.h"


enum class ServerRunningMode
{
    Persistent,   // Default: handle multiple requests
    SingleRequest // Handle exactly one request, then stop
};

struct Config
{
    uint16_t http_port = DEFAULT_HTTP_PORT; // default port
    uint16_t https_port = DEFAULT_HTTPS_PORT;
    ServerRunningMode running_mode = ServerRunningMode::Persistent;
    spdlog::level::level_enum log_level = spdlog::level::level_enum::info; // default log level
    LoggerType logger_type = LoggerType::Console;
    std::string srv_cert_path = "cert/server.crt";
    std::string srv_private_key_path = "cert/server.key";
    bool ignore_certificate_verification = false;
    std::string upstream_host = "api.binance.com";
    uint16_t upstream_port  = 443;
    bool allow_https_over_http_port = false;
    std::string config_path = "";
    uint16_t worker_threads = 3;
};