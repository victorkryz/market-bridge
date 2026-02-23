#pragma once

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <atomic>

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
    Server(unsigned short port, ServerRunningMode running_mode = ServerRunningMode::Persistent);
    int run();

private:
    void do_accept();

    ServerRunningMode running_mode_;
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
};
