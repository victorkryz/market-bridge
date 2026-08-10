#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "common/app.h"
#include "common/config.h"

namespace
{
    std::string to_string(ServerRunningMode mode)
    {
        switch (mode)
        {
        case ServerRunningMode::Persistent:
            return "persist";
        case ServerRunningMode::SingleRequest:
            return "single-request";
        }

        return "unknown";
    }

    std::string to_string(LoggerType type)
    {
        switch (type)
        {
        case LoggerType::Console:
            return "console";
        case LoggerType::File:
            return "file";
        }

        return "unknown";
    }

    std::string to_string(spdlog::level::level_enum level)
    {
        const auto level_name = spdlog::level::to_string_view(level);
        return std::string(level_name.data(), level_name.size());
    }

    std::string to_string(bool value)
    {
        return value ? "true" : "false";
    }

    std::string endpoint(std::string host, uint16_t port)
    {
        return host + ":" + std::to_string(port);
    }
}

std::string make_startup_table(const Config& cfg)
{
    using Row = std::pair<std::string, std::string>;

    std::vector<Row> rows{
        {"Version", app_version},
        {"HTTP listener", endpoint("localhost", cfg.server.http_port)},
        {"HTTPS listener", endpoint("localhost", cfg.server.https_port)},
        {"Upstream", endpoint(cfg.upstream.host, cfg.upstream.port)},
        {"Run mode", to_string(cfg.server.run_mode)},
        {"Log output", to_string(cfg.logging.output)},
        {"Log level", to_string(cfg.logging.level)},
        {"TLS certificate", cfg.tls.certificate},
        {"TLS private key", cfg.tls.private_key},
        {"Ignore upstream cert", to_string(cfg.upstream.ignore_certificate_verification)},
        {"Allow HTTPS over HTTP port", to_string(cfg.server.allow_https_over_http_port)},
        {"Throttling enabled", to_string(cfg.throttling.enabled)},
    };

    if (cfg.throttling.enabled)
    {
        rows.emplace_back("Throttling requests/second", std::to_string(cfg.throttling.requests_per_second));
        rows.emplace_back("Throttling burst size", std::to_string(cfg.throttling.burst_size));
    }
    
    constexpr std::size_t title_padding = 2;
    std::size_t key_width = 0;
    std::size_t value_width = 0;

    for (const auto& [key, value] : rows)
    {
        key_width = std::max(key_width, key.size());
        value_width = std::max(value_width, value.size());
    }

    const std::string title = " " + std::string(app_name) + " startup ";
    value_width = std::max(value_width, title.size() + title_padding);

    const auto line = [&]()
    {
        return "+" + std::string(key_width + 2, '-') +
               "+" + std::string(value_width + 2, '-') + "+";
    };

    const std::size_t inner_width = key_width + value_width + 5;
    const std::size_t left_title_padding = (inner_width - title.size()) / 2;
    const std::size_t right_title_padding = inner_width - title.size() - left_title_padding;

    std::ostringstream out;
    out << line() << '\n'
        << "|" << std::string(left_title_padding, ' ') << title
        << std::string(right_title_padding, ' ') << "|\n"
        << line() << '\n';

    for (const auto& [key, value] : rows)
    {
        out << "| " << std::left << std::setw(static_cast<int>(key_width)) << key
            << " | " << std::left << std::setw(static_cast<int>(value_width)) << value
            << " |\n";
    }

    out << line();
    return out.str();
}
