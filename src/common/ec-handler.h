#pragma once

#include <source_location>
#include <asio.hpp>
#include <string_view>
#include <spdlog/fmt/fmt.h>

#include "logs/logger.h"

inline bool is_eof(const asio::error_code& ec) noexcept { return (ec == asio::error::eof); }
inline bool is_aborted(const asio::error_code& ec) noexcept { return (ec == asio::error::operation_aborted); }
inline bool is_cancelled(const asio::error_code& ec) noexcept { return is_aborted(ec); }

inline bool check_ec(const asio::error_code& ec, std::string_view context) noexcept
{
    bool result(!ec);
    if (!result)
    {
        if (is_cancelled(ec))
            gl_logger->trace("{}: {} code: {}", context, ec.message(), ec.value());
        else
            gl_logger->error("{}: {} code: {}", context, ec.message(), ec.value());
    }
    return result;
}


inline bool check_ec(const asio::error_code& ec, 
                      std::source_location loc = std::source_location::current()) noexcept
{
    return check_ec(ec, fmt::format("{}:{}", loc.file_name(), loc.function_name())); 
}
