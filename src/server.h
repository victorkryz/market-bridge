#pragma once

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl.hpp>
#include <atomic>

#include "common/session.h"

enum class ServerRunningMode
{
    Persistent,   // Default: handle multiple requests
    SingleRequest // Handle exactly one request, then stop
};

class Server
{
    static inline std::atomic<uint64_t> session_id_gen{1};

    static uint64_t generate_session_id()
    {
        return session_id_gen.fetch_add(1, std::memory_order_relaxed);
    }

public:
    Server(unsigned short http_port, unsigned short https_port, ServerRunningMode running_mode = ServerRunningMode::Persistent);
    int run();
    void schedule_shutdown();

private:
    void listener(asio::ip::tcp::acceptor& acceptor, std::function<void(asio::ip::tcp::socket)> completion_handler);
    template <typename T>
    void launch_http_session(T&& stream);
    void dispatch_request(asio::ip::tcp::socket socket);
    void init_acceptors();
    void init_ssl_context();
    void install_listeners();
    void install_signals_handler();
    void ssl_handshake(asio::ip::tcp::socket&& socket);
    void on_ssl_handshake_done(asio::ssl::stream<asio::ip::tcp::socket>&& stream);
    void stop_sessions();
    void close_acceptors();
    
private:
    ServerRunningMode running_mode_;
    asio::io_context io_;
    asio::ssl::context ssl_context_;
    bool ssl_context_init_done_ = false;
    uint16_t http_port_;
    uint16_t https_port_;
    std::unique_ptr<asio::ip::tcp::acceptor> http_acceptor_;
    std::unique_ptr<asio::ip::tcp::acceptor> https_acceptor_;
    asio::signal_set signals_;
    std::atomic<bool> shutdown_pending_ = false;
    std::vector<std::weak_ptr<Session>> sessions_;
    std::string cert_file_path = "cert/server.crt";
    std::string private_key_path_path = "cert/server.key";
};

