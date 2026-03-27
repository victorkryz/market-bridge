#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <atomic>

namespace session::helper
{
    using asio::ip::tcp;

    template <typename Stream>
    inline auto& lowest_socket(Stream& stream)
    {
        if constexpr (std::is_same_v<Stream, tcp::socket>)
            return stream;
        else if constexpr (std::is_same_v<Stream, asio::ssl::stream<tcp::socket>>)
            return stream.lowest_layer();
    }

    template <typename T>
    inline void shutdown_socket(T& socket)
    {
        asio::error_code ec_formal;
        socket.cancel(ec_formal);
        socket.shutdown(tcp::socket::shutdown_both, ec_formal);
        socket.close(ec_formal);
    }

    template <typename S>
    class SessionBase
    {
        protected:
            explicit SessionBase(uint64_t id) : id_(id) {}

            bool is_stopped() const noexcept
            {
                return stopped_.load(std::memory_order_acquire); 
            }

            bool request_stop() noexcept
            {
                return !stopped_.exchange(true, std::memory_order_acq_rel);
            }

            uint64_t get_session_id() const noexcept
            {
                return id_;
            }

        protected:
            uint64_t id_{0};
            std::atomic<bool> stopped_ {false};
    };

}; // namespace session::helper