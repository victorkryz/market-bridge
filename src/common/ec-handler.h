#pragma once

#include <asio.hpp>
#include <string_view>

#include "logs/logger.h"

inline bool is_eof(const asio::error_code& ec) noexcept { return (ec == asio::error::eof); }

inline bool check_ec(const asio::error_code& ec, std::string_view context) noexcept
{
    bool result(!ec);
    if (!result)
        gl_logger->error("{}: {} code: {}", context, ec.message(), ec.value());
    return result;
}
    