#pragma once

#include <spdlog/spdlog.h>

enum class LoggerType 
{
    Console,
    File
};

inline std::shared_ptr<spdlog::logger> gl_logger;




