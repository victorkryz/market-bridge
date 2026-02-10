#include "server.h"
#include "http-session.h"
#include "logs/logger.h"
#include "common/ec-handler.h"

Server::Server(unsigned short port) : acceptor_(io_, asio::ip::tcp::endpoint(tcp::v4(), port)) {}

void Server::run()
{
    gl_logger->trace("Server running ...");

    do_accept();

    std::vector<std::thread> threads;

    for (size_t i = 0; i < 3; i++)
        threads.emplace_back([this]() { io_.run(); });

    for (auto& th : threads)
        th.join();
}

void Server::do_accept()
{
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            if (check_ec(ec, __func__ ))
            {
                gl_logger->trace("Server accepted connection");

                auto session = std::make_shared<HTTPSession>(io_, std::move(socket));
                session->start();
            }
            do_accept();
        });
}
