#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <iostream>

#include "logs/logger.h"
#include <spdlog/spdlog.h>
#include <cxxopts.hpp>
#include "server.h"
#include "common/app.h"

bool gl_show_usage_called(false);

namespace
{
    std::vector<std::string> argument_storage;
    std::vector<char*> in_arguments;

    void set_arguments(std::initializer_list<std::string> args)
    {
        argument_storage.assign(args);

        in_arguments.clear();
        in_arguments.reserve(argument_storage.size());

        for (auto& arg : argument_storage)
        {
            in_arguments.push_back(arg.data());
        }
    }
} // namespace

TEST_SUITE("testing the command line processor suite")
{
    TEST_CASE("testing the command line processor case")
    {
        Config cfg;
        gl_show_usage_called = false;

        SUBCASE("empty command line")
        {
            set_arguments({"app"});

            const auto [exit_code, usage_requested] =
                process_arguments(in_arguments.size(), in_arguments.data(), cfg);

            CHECK(exit_code == 0);

            CHECK(cfg.http_port == default_http_port);
            CHECK(cfg.log_level == spdlog::level::level_enum::info);
            CHECK(cfg.running_mode == ServerRunningMode::Persistent);

            CHECK(!usage_requested);
            CHECK(!gl_show_usage_called);
        }

        SUBCASE("--help")
        {
            set_arguments({"app", "--help"});

            const auto [exit_code, usage_requested] =
                process_arguments(in_arguments.size(), in_arguments.data(), cfg);

            CHECK(exit_code == 0);
            CHECK(usage_requested);
            CHECK(gl_show_usage_called);
        }
        SUBCASE("--port")
        {
            constexpr auto in_port = 8585u;
            doctest::String s = doctest::toString(in_port);

            set_arguments({"app", "--http-port", s.c_str()});

            const auto [exit_code, usage_requested] =
                process_arguments(in_arguments.size(), in_arguments.data(), cfg);

            CHECK(exit_code == 0);

            CHECK(cfg.http_port == in_port);
            CHECK(cfg.log_level == spdlog::level::level_enum::info);
            CHECK(cfg.running_mode == ServerRunningMode::Persistent);

            CHECK(!usage_requested);
            CHECK(!gl_show_usage_called);
        }
    }
}

void show_usage(const cxxopts::Options& options)
{
    gl_show_usage_called = true;
}