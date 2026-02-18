#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <iostream>

#include "logs/logger.h"
#include <spdlog/spdlog.h>
#include <cxxopts.hpp>
#include "server.h"
#include "common/app.h"

bool gl_show_usage_called(false);

TEST_SUITE("testing the command line processor suite")
{
    TEST_CASE("testing the command line processor case")
    {
        ClArguments args;
        gl_show_usage_called = false;

        SUBCASE("empty command line")
        {
            std::vector<char*> in_arguments = {"app"};

            const auto [exit_code, usage_requested] =
                process_arguments(in_arguments.size(), in_arguments.data(), args);

            CHECK(exit_code == 0);

            CHECK(args.port == default_http_port);
            CHECK(args.log_level == spdlog::level::level_enum::info);
            CHECK(args.running_mode == ServerRunningMode::Persistent);

            CHECK(!usage_requested);
            CHECK(!gl_show_usage_called);
        }

        SUBCASE("--help")
        {
            std::vector<char*> in_arguments = {"app", "--help"};

            const auto [exit_code, usage_requested] =
                process_arguments(in_arguments.size(), in_arguments.data(), args);

            CHECK(exit_code == 0);
            CHECK(usage_requested);
            CHECK(gl_show_usage_called);
        }
        SUBCASE("--port")
        {
            constexpr auto in_port = 8585u;
            doctest::String s = doctest::toString(in_port);

            std::vector<char*> in_arguments = {"app", "--port", s.c_str()};

            const auto [exit_code, usage_requested] =
                process_arguments(in_arguments.size(), in_arguments.data(), args);

            CHECK(exit_code == 0);

            CHECK(args.port == in_port);
            CHECK(args.log_level == spdlog::level::level_enum::info);
            CHECK(args.running_mode == ServerRunningMode::Persistent);

            CHECK(!usage_requested);
            CHECK(!gl_show_usage_called);
        }
    }
}

void show_usage(const cxxopts::Options& options)
{
    gl_show_usage_called = true;
}