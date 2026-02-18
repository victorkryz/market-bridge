#include <gtest/gtest.h>

#include <iostream>

// clang-format off
#include "logs/logger.h"
#include <spdlog/spdlog.h>
#include <cxxopts.hpp>
#include "server.h"
#include "common/app.h"
#include "spdlog/common.h"
// clang-format on

bool gl_show_usage_called(false);

void show_usage(const cxxopts::Options& options)
{
    gl_show_usage_called = true;
}

struct TestBase
{
    void SetUp()
    {
        gl_show_usage_called = false;
    };

    void TearDown(){};

    ClArguments out_args_;
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
    in_args_ = {""};

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.port, default_http_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}

TEST_F(CommandLineTS, HelpArgTest)
{
    in_args_ = {"", "--help"};

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(usage_requested);
    EXPECT_TRUE(gl_show_usage_called);
}

TEST_F(CommandLineTS, PortArgTest)
{
    constexpr auto in_port = 8585u;
    std::string s_port = testing::PrintToString(in_port);

    in_args_ = {"", "--port", s_port.data()};

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);

    EXPECT_EQ(out_args_.port, in_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);

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
    in_args_ = {"", "--log_level", log_level_str.data()};

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(out_args_.port, default_http_port);
    EXPECT_EQ(out_args_.running_mode, ServerRunningMode::Persistent);

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
    in_args_ = {"", "--log_type", logger_type_str.data()};

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(out_args_.port, default_http_port);
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
                             RunModePair("single_request", ServerRunningMode::SingleRequest)));

TEST_P(RunMode_TS, RunMode)
{
    auto [run_mode_str, run_mode] = GetParam();
    in_args_ = {"", "--run_mode", run_mode_str.data()};

    const auto [exit_code, usage_requested] =
        process_arguments(in_args_.size(), in_args_.data(), out_args_);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(out_args_.port, default_http_port);
    EXPECT_EQ(out_args_.log_level, spdlog::level::level_enum::info);
    EXPECT_EQ(out_args_.logger_type, LoggerType::File);

    EXPECT_EQ(out_args_.running_mode, run_mode);

    EXPECT_FALSE(usage_requested);
    EXPECT_FALSE(gl_show_usage_called);
}
