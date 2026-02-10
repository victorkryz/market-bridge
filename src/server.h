#pragma once

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

class Server 
{
    public:
        Server(unsigned short port);
        void run();

    private:
        void do_accept();

        asio::io_context io_;
        asio::ip::tcp::acceptor acceptor_;
};

