#pragma once

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

enum class ServerRunningMode
{
    Persistent,   // Default: handle multiple requests
    SingleRequest // Handle exactly one request, then stop
};

class Server
{
public:
public:
    Server(unsigned short port, ServerRunningMode running_mode = ServerRunningMode::Persistent);
    int run();

private:
    void do_accept();

    ServerRunningMode running_mode_;
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
};
