#pragma once

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl.hpp>
#include <atomic>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

#include "common/config.h"
#include "common/session.h"
#include "utils/session-helper.h"

using asio::awaitable;
using asio::use_awaitable;
namespace this_coro = asio::this_coro;

class Server
{
    static inline std::atomic<uint64_t> session_id_gen{1};

    static uint64_t generate_session_id()
    {
        return session_id_gen.fetch_add(1, std::memory_order_relaxed);
    }

public:
    Server(const Config& cfg);
    int run();
    void schedule_shutdown();

private:
    awaitable<void> listener(asio::ip::tcp::acceptor& acceptor,
                             std::function<void(asio::ip::tcp::socket)> completion_handler);
    asio::awaitable<void> dispatch_http_request(asio::ip::tcp::socket socket);
    template <session::helper::HttpStream T>
    void launch_http_session(T&& stream);
    void init_acceptors();
    void init_ssl_context();
    void install_listeners();
    void uninstall_listeners();
    void install_signals_handler();
    void uninstall_signals_handler();
    void ssl_handshake(asio::ip::tcp::socket&& socket);
    void on_ssl_handshake_done(asio::ssl::stream<asio::ip::tcp::socket>&& stream);
    void stop_sessions();
    void close_acceptors();
    bool register_session(std::shared_ptr<Session> session);

private:
    ServerRunningMode running_mode_;
    asio::io_context io_;
    asio::ssl::context ssl_context_;
    std::once_flag ssl_context_init_flag_;
    std::unique_ptr<asio::ip::tcp::acceptor> http_acceptor_;
    std::unique_ptr<asio::ip::tcp::acceptor> https_acceptor_;
    asio::signal_set signals_;
    std::atomic<bool> shutdown_pending_ = false;
    std::vector<std::weak_ptr<Session>> sessions_;
    std::mutex session_mtx_, acceptor_mtx_;
    const Config& cfg_;
};
