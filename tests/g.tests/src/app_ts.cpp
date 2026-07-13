#include <gtest/gtest.h>

#include <iostream>

// clang-format off
#include "logs/logger.h"
#include <spdlog/spdlog.h>
#include <cxxopts.hpp>
#include "server.h"
#include "common/app.h"
#include "spdlog/common.h"
#include <string_view>
// clang-format on

using namespace std::literals;

bool gl_show_usage_called(false);

constexpr std::string_view default_json_config = R"({
  "server": {
    "http_port": 8080,
    "https_port": 8443,
    "run_mode": "persist",
    "allow_https_over_http_port": false
  },
  "tls": {
    "certificate": "cert/server.crt",
    "private_key": "cert/server.key"
  },
  "upstream": {
    "host": "api.binance.com",
    "port": 443,
    "ignore_certificate_verification": false
  },
  "logging": {
    "output": "console",
    "level": "info"
  }
})";

void show_usage(const cxxopts::Options& options)
{
    gl_show_usage_called = true;
}

struct TestBase
{
    inline static constexpr LoggerType default_log_type = LoggerType::Console;
    inline static constexpr std::string_view default_srv_cert_path = "cert/server.crt";
    inline static constexpr std::string_view default_srv_private_key_path = "cert/server.key";

    void SetUp()
    {
        gl_show_usage_called = false;
        in_args_.clear();
        argument_storage_.clear();
    };

    void TearDown() {};

    void set_arguments(std::initializer_list<std::string> args)
    {
        argument_storage_.assign(args);

        in_args_.clear();
        in_args_.reserve(argument_storage_.size());

        for (auto& arg : argument_storage_)
        {
            in_args_.push_back(arg.data());
        }
    }

    Config out_args_;
    Config default_args_;
    std::vector<std::string> argument_storage_;
    std::vector<char*> in_args_;
};

class CommandLineTS : protected TestBase,
                      public ::testing::Test
{
    void SetUp() override
    {
        TestBase::SetUp();
    }

    void TearDown() override
    {
        TestBase::TearDown();
    };
};

using LogLevelPair = std::pair<std::string, spdlog::level::level_enum>;

class LogLevel_TS : protected TestBase,
                    public ::testing::TestWithParam<LogLevelPair>
{
    void SetUp() override
    {
        TestBase::SetUp();
    }

    void TearDown() override
    {
        TestBase::TearDown();
    };
};

using LogTypePair = std::pair<std::string, LoggerType>;

class LogType_TS : protected TestBase,
                   public ::testing::TestWithParam<LogTypePair>
{
    void SetUp() override
    {
        TestBase::SetUp();
    }

    void TearDown() override
    {
        TestBase::TearDown();
    };
};

using RunModePair = std::pair<std::string, ServerRunningMode>;

class RunMode_TS : protected TestBase,
                   public ::testing::TestWithParam<RunModePair>
{
    void SetUp() override
    {
        TestBase::SetUp();
    }

    void TearDown() override
    {
        TestBase::TearDown();
    };
};

TEST_F(CommandLineTS, EmptyLineTest)
{
    set_arguments({""});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.https_port, default_https_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, HelpArgTest)
{
    set_arguments({"", "--help"});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(usage_requested);
    EXPECT_TRUE(gl_show_usage_called);
}

TEST_F(CommandLineTS, HttpPortArgTest)
{
    constexpr auto in_port = 8585u;
    const std::string str_port = testing::PrintToString(in_port);

    set_arguments({"", "--http-port", str_port.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    
    EXPECT_EQ(out_args_.http_port, in_port);
    EXPECT_EQ(out_args_.https_port, default_https_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, default_args_.upstream_host);
    EXPECT_EQ(out_args_.upstream_port, default_args_.upstream_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, HttpsPortArgTest)
{
    constexpr auto in_port = 9443u;
    const std::string str_port = testing::PrintToString(in_port);

    set_arguments({"", "--https-port", str_port.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.https_port, in_port);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, default_args_.upstream_host);
    EXPECT_EQ(out_args_.upstream_port, default_args_.upstream_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, CertPathArgTest)
{
    const std::string s_cert = "/etc/ssl/certs/server.crt",
                      s_private_key = "/etc/ssl/certs/server.key";

    set_arguments({"", "--cert-path", s_cert.data(), "--private-key", s_private_key.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.srv_cert_path, s_cert);
    EXPECT_EQ(out_args_.srv_private_key_path, s_private_key);

    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.https_port, default_https_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, default_args_.upstream_host);
    EXPECT_EQ(out_args_.upstream_port, default_args_.upstream_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, IgnoreCertificateVerificationArgTest)
{
    set_arguments({"", "--ignore-cert-verification"});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_TRUE(out_args_.ignore_certificate_verification);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}


TEST_F(CommandLineTS, AllowHttpsOverHttpPortArgTest)
{
    set_arguments({"", "--allow-https-over-http-port"});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_TRUE(out_args_.allow_https_over_http_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}


TEST_F(CommandLineTS, UpstreamHostArgTest)
{
    constexpr auto in_host = "api.test.com"sv;
    set_arguments({"", "--upstream-host", in_host.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.upstream_host, in_host);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_port, default_args_.upstream_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}


TEST_F(CommandLineTS, UpstreamPortArgTest)
{
    constexpr auto in_port = 8443u;
    const std::string str_port = testing::PrintToString(in_port);

    set_arguments({"", "--upstream-port", str_port.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.upstream_port, in_port);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, default_args_.upstream_host);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}


INSTANTIATE_TEST_SUITE_P(LogLevelArg,
                         LogLevel_TS,
                         ::testing::Values(
                             LogLevelPair("info", spdlog::level::level_enum::info),
                             LogLevelPair("warning", spdlog::level::level_enum::warn),
                             LogLevelPair("debug", spdlog::level::level_enum::debug),
                             LogLevelPair("critical", spdlog::level::level_enum::critical),
                             LogLevelPair("trace", spdlog::level::level_enum::trace),
                             LogLevelPair("off", spdlog::level::level_enum::off),
                             LogLevelPair("error", spdlog::level::level_enum::err)));

TEST_P(LogLevel_TS, LogLevelArg)
{
    auto [log_level_str, spd_log_level] = GetParam();
    set_arguments({"", "--log-level", log_level_str.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);

    EXPECT_EQ(out_args_.log_level, spd_log_level);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

INSTANTIATE_TEST_SUITE_P(LoggerType,
                         LogType_TS,
                         ::testing::Values(
                             LogTypePair("file", LoggerType::File),
                             LogTypePair("console", LoggerType::Console),
                             LogTypePair("con", LoggerType::Console)));

TEST_P(LogType_TS, LoggerType)
{
    auto [logger_type_str, logger_type] = GetParam();
    set_arguments({"", "--log-output", logger_type_str.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);

    EXPECT_EQ(out_args_.logger_type, logger_type);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

INSTANTIATE_TEST_SUITE_P(RunMode,
                         RunMode_TS,
                         ::testing::Values(
                             RunModePair("persist", ServerRunningMode::Persistent),
                             RunModePair("single-request", ServerRunningMode::SingleRequest)));

TEST_P(RunMode_TS, RunMode)
{
    auto [run_mode_str, run_mode] = GetParam();
    set_arguments({"", "--run-mode", run_mode_str.data()});

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.logger_type, default_log_type);

    EXPECT_EQ(out_args_.running_mode, run_mode);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}


TEST_F(CommandLineTS, DefaultJsonConfigTest)
{
    constexpr auto str_config = "config.json"sv;
    set_arguments({"", "--config", str_config.data()});

    LoadFromString cfg_loader(default_json_config);

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), cfg_loader, out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.config_path, str_config);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.https_port, default_https_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);
    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, default_args_.upstream_host);
    EXPECT_EQ(out_args_.upstream_port, default_args_.upstream_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, CustomJsonConfigTest)
{
    constexpr auto str_config = "fake_config.json"sv;
    set_arguments({"", "--config", str_config.data()});

    constexpr std::string_view json_config = R"({
        "server": {
            "http_port": 9080,
            "https_port": 9443,
            "run_mode": "persist",
            "allow_https_over_http_port": true
        },
        "logging": {
            "output": "console",
            "level": "trace"
        }
        })";

    LoadFromString cfg_loader(json_config);

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), cfg_loader, out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.config_path, str_config);
    EXPECT_EQ(out_args_.http_port, 9080u);
    EXPECT_EQ(out_args_.https_port, 9443u);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::trace);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, default_log_type);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);

    EXPECT_FALSE(out_args_.ignore_certificate_verification);
    EXPECT_TRUE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, default_args_.upstream_host);
    EXPECT_EQ(out_args_.upstream_port, default_args_.upstream_port);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, CustomJsonConfigTest2)
{
    constexpr auto str_config = "fake_config.json"sv;
    set_arguments({"", "--config", str_config.data()});
    
    constexpr std::string_view json_config = R"({

        "upstream": {
            "host": "api.test.com",
            "port": 9443,
            "ignore_certificate_verification": true
        },
        "logging": {
            "output": "file",
            "level": "off"
        }
        })";

    LoadFromString cfg_loader(json_config);

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), cfg_loader, out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.config_path, str_config);
    EXPECT_EQ(out_args_.http_port, default_http_port);
    EXPECT_EQ(out_args_.https_port, default_https_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::off);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);
    EXPECT_EQ(out_args_.logger_type, LoggerType::File);
    EXPECT_EQ(out_args_.srv_cert_path, default_srv_cert_path);
    EXPECT_EQ(out_args_.srv_private_key_path, default_srv_private_key_path);

    EXPECT_TRUE(out_args_.ignore_certificate_verification);
    EXPECT_FALSE(out_args_.allow_https_over_http_port);

    EXPECT_EQ(out_args_.upstream_host, "api.test.com");
    EXPECT_EQ(out_args_.upstream_port, 9443u);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}