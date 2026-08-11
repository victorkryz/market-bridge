#include "config/config_json.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace
{
    using json = nlohmann::json;
    using namespace std::literals;

    constexpr auto server_key = "server"sv;
    constexpr auto tls_key = "tls"sv;
    constexpr auto upstream_key = "upstream"sv;
    constexpr auto logging_key = "logging"sv;
    constexpr auto throttling_key = "throttling"sv;

    constexpr auto http_port_key = "http_port"sv;
    constexpr auto https_port_key = "https_port"sv;
    constexpr auto run_mode_key = "run_mode"sv;
    constexpr auto allow_https_over_http_port_key = "allow_https_over_http_port"sv;
    constexpr auto certificate_key = "certificate"sv;
    constexpr auto private_key_key = "private_key"sv;
    constexpr auto host_key = "host"sv;
    constexpr auto port_key = "port"sv;
    constexpr auto ignore_certificate_verification_key =
        "ignore_certificate_verification"sv;
    constexpr auto output_key = "output"sv;
    constexpr auto level_key = "level"sv;
    constexpr auto enabled_key = "enabled"sv;
    constexpr auto requests_per_second_key = "requests_per_second"sv;
    constexpr auto burst_size_key = "burst_size"sv;

    constexpr auto configuration_root_name = "configuration root"sv;
    constexpr auto root_name = "root"sv;
    constexpr auto server_http_port_name = "server.http_port"sv;
    constexpr auto server_https_port_name = "server.https_port"sv;
    constexpr auto server_run_mode_name = "server.run_mode"sv;
    constexpr auto server_allow_https_over_http_port_name =
        "server.allow_https_over_http_port"sv;
    constexpr auto tls_certificate_name = "tls.certificate"sv;
    constexpr auto tls_private_key_name = "tls.private_key"sv;
    constexpr auto upstream_host_name = "upstream.host"sv;
    constexpr auto upstream_port_name = "upstream.port"sv;
    constexpr auto upstream_ignore_certificate_verification_name =
        "upstream.ignore_certificate_verification"sv;
    constexpr auto logging_output_name = "logging.output"sv;
    constexpr auto logging_level_name = "logging.level"sv;
    constexpr auto throttling_enabled_name = "throttling.enabled"sv;
    constexpr auto throttling_requests_per_second_name =
        "throttling.requests_per_second"sv;
    constexpr auto throttling_burst_size_name = "throttling.burst_size"sv;

    constexpr auto persistent_value = "persist"sv;
    constexpr auto single_request_value = "single-request"sv;
    constexpr auto console_value = "console"sv;
    constexpr auto console_short_value = "con"sv;
    constexpr auto file_value = "file"sv;
    constexpr auto trace_value = "trace"sv;
    constexpr auto debug_value = "debug"sv;
    constexpr auto info_value = "info"sv;
    constexpr auto warning_value = "warning"sv;
    constexpr auto error_value = "error"sv;
    constexpr auto critical_value = "critical"sv;
    constexpr auto off_value = "off"sv;

    constexpr std::int64_t min_port = 1;
    constexpr std::int64_t max_port = 65535;

    void obtain_object(const json& value, std::string_view name)
    {
        if (!value.is_object())
            throw std::runtime_error(std::string(name) + " expected be a JSON object");
    }

    void validate_keys(const json& object,
                       std::initializer_list<std::string_view> allowed,
                       std::string_view name)
    {
        for (const auto& [key, _] : object.items())
        {
            if (std::ranges::find(allowed, key) == allowed.end())
                throw std::runtime_error("unknown configuration property: " +
                                         std::string(name) + "." + key);
        }
    }

    uint16_t read_port(const json& value, std::string_view name)
    {
        if (!value.is_number_unsigned() && !value.is_number_integer())
            throw std::runtime_error(std::string(name) + " expected be an integer");

        const auto port = value.get<std::int64_t>();
        if (port < min_port || port > max_port)
            throw std::runtime_error(std::string(name) + " expected be between " +
                                     std::to_string(min_port) + " and " +
                                     std::to_string(max_port));
        return static_cast<uint16_t>(port);
    }

    std::string read_string(const json& value, std::string_view name, bool allow_empty = true)
    {
        if (!value.is_string())
            throw std::runtime_error(std::string(name) + " expected be a string");

        auto result = value.get<std::string>();
        if (!allow_empty && result.empty())
            throw std::runtime_error(std::string(name) + " expected not be empty");
        return result;
    }

    bool read_bool(const json& value, std::string_view name)
    {
        if (!value.is_boolean())
            throw std::runtime_error(std::string(name) + " expected be a boolean");
        return value.get<bool>();
    }

    uint32_t read_uint32(const json& value, std::string_view name)
    {
        if (!value.is_number_unsigned() && !value.is_number_integer())
            throw std::runtime_error(std::string(name) + " expected be an integer");

        if (value.is_number_unsigned())
        {
            const auto number = value.get<std::uint64_t>();
            if (number <= std::numeric_limits<uint32_t>::max())
                return static_cast<uint32_t>(number);
        }
        else
        {
            const auto number = value.get<std::int64_t>();
            if (number >= 0 &&
                static_cast<std::uint64_t>(number) <= std::numeric_limits<uint32_t>::max())
                return static_cast<uint32_t>(number);
        }

        throw std::runtime_error(std::string(name) +
                                 " expected be between 0 and " +
                                 std::to_string(std::numeric_limits<uint32_t>::max()));
    }

    ServerRunningMode read_run_mode(const json& value)
    {
        const auto mode = read_string(value, server_run_mode_name);
        if (mode == persistent_value)
            return ServerRunningMode::Persistent;
        if (mode == single_request_value)
            return ServerRunningMode::SingleRequest;
        throw std::runtime_error(std::string(server_run_mode_name) + " expected be '" +
                                 std::string(persistent_value) + "' or '" +
                                 std::string(single_request_value) + "'");
    }

    LoggerType read_log_output(const json& value)
    {
        const auto output = read_string(value, logging_output_name);
        if (output == console_value || output == console_short_value)
            return LoggerType::Console;
        if (output == file_value)
            return LoggerType::File;
        throw std::runtime_error(std::string(logging_output_name) + " expected be '" +
                                 std::string(console_value) + "', '" +
                                 std::string(console_short_value) + "', or '" +
                                 std::string(file_value) + "'");
    }

    spdlog::level::level_enum read_log_level(const json& value)
    {
        const auto level = read_string(value, logging_level_name);
        if (level == trace_value)
            return spdlog::level::trace;
        if (level == debug_value)
            return spdlog::level::debug;
        if (level == info_value)
            return spdlog::level::info;
        if (level == warning_value)
            return spdlog::level::warn;
        if (level == error_value)
            return spdlog::level::err;
        if (level == critical_value)
            return spdlog::level::critical;
        if (level == off_value)
            return spdlog::level::off;
        throw std::runtime_error(std::string(logging_level_name) + " expected be '" +
                                 std::string(trace_value) + "', '" +
                                 std::string(debug_value) + "', '" +
                                 std::string(info_value) + "', '" +
                                 std::string(warning_value) + "', '" +
                                 std::string(error_value) + "', '" +
                                 std::string(critical_value) + "', or '" +
                                 std::string(off_value) + "'");
    }

    void merge_json_config(const json& root, Config& config)
    {
        obtain_object(root, configuration_root_name);
        validate_keys(root, {server_key, tls_key, upstream_key, logging_key, throttling_key},
                      root_name);

        if (const auto it = root.find(server_key); it != root.end())
        {
            obtain_object(*it, server_key);
            validate_keys(*it,
                          {http_port_key, https_port_key, run_mode_key,
                           allow_https_over_http_port_key},
                          server_key);
            if (it->contains(http_port_key))
                config.server.http_port = read_port(it->at(http_port_key), server_http_port_name);
            if (it->contains(https_port_key))
                config.server.https_port = read_port(it->at(https_port_key), server_https_port_name);
            if (it->contains(run_mode_key))
                config.server.run_mode = read_run_mode(it->at(run_mode_key));
            if (it->contains(allow_https_over_http_port_key))
                config.server.allow_https_over_http_port =
                    read_bool(it->at(allow_https_over_http_port_key),
                              server_allow_https_over_http_port_name);
        }

        if (const auto it = root.find(tls_key); it != root.end())
        {
            obtain_object(*it, tls_key);
            validate_keys(*it, {certificate_key, private_key_key}, tls_key);
            if (it->contains(certificate_key))
                config.tls.certificate =
                    read_string(it->at(certificate_key), tls_certificate_name);
            if (it->contains(private_key_key))
                config.tls.private_key =
                    read_string(it->at(private_key_key), tls_private_key_name);
        }

        if (const auto it = root.find(upstream_key); it != root.end())
        {
            obtain_object(*it, upstream_key);
            validate_keys(*it,
                          {host_key, port_key, ignore_certificate_verification_key},
                          upstream_key);
            if (it->contains(host_key))
                config.upstream.host =
                    read_string(it->at(host_key), upstream_host_name, false);
            if (it->contains(port_key))
                config.upstream.port = read_port(it->at(port_key), upstream_port_name);
            if (it->contains(ignore_certificate_verification_key))
                config.upstream.ignore_certificate_verification =
                    read_bool(it->at(ignore_certificate_verification_key),
                              upstream_ignore_certificate_verification_name);
        }

        if (const auto it = root.find(logging_key); it != root.end())
        {
            obtain_object(*it, logging_key);
            validate_keys(*it, {output_key, level_key}, logging_key);
            if (it->contains(output_key))
                config.logging.output = read_log_output(it->at(output_key));
            if (it->contains(level_key))
                config.logging.level = read_log_level(it->at(level_key));
        }

        if (const auto it = root.find(throttling_key); it != root.end())
        {
            obtain_object(*it, throttling_key);
            validate_keys(*it,
                          {enabled_key, requests_per_second_key, burst_size_key},
                          throttling_key);
            if (it->contains(enabled_key))
                config.throttling.enabled =
                    read_bool(it->at(enabled_key), throttling_enabled_name);
            if (it->contains(requests_per_second_key))
                config.throttling.requests_per_second =
                    read_uint32(it->at(requests_per_second_key),
                                throttling_requests_per_second_name);
            if (it->contains(burst_size_key))
                config.throttling.burst_size =
                    read_uint32(it->at(burst_size_key),
                                throttling_burst_size_name);
        }
    }

    template <typename Loader>
    json load_from_stream(Loader obtain_stream)
    {
        try
        {
            auto s = obtain_stream();

            json document;
            s >> document;

            return document;
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("failed to load configuration '" + obtain_stream.source() +
                                     "': " + e.what());
        }
    }
}

template <typename Loader>
void load_json_config(Loader loader, Config& config)
{
    try
    {
        json document = load_from_stream(loader);
        Config merged_config = config;
        merge_json_config(document, merged_config);
        config = std::move(merged_config);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("failed to load configuration '" + loader.source() +
                                 "': " + e.what());
    }
}

std::ifstream obtain_stream_from_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("cannot open file");
    return input;
}

template void load_json_config<LoadFromFile>(LoadFromFile loader, Config& config);
template void load_json_config<LoadFromString>(LoadFromString loader, Config& config);
