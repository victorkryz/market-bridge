#include "server.h"
#include "common/ec-handler.h"
#include "http-session.h"
#include "logs/logger.h"

Server::Server(unsigned short port, ServerRunningMode running_mode) : running_mode_(running_mode),
                                                                      acceptor_(io_, asio::ip::tcp::endpoint(tcp::v4(), port)) {}

int Server::run()
{
    gl_logger->info("Server running ...");

    do_accept();

    std::vector<std::thread> threads;

    for (size_t i = 0; i < 3; i++)
        threads.emplace_back([this]()
                             { io_.run(); });

    for (auto& th : threads)
        th.join();

    gl_logger->info("Server finished");

    return 0;
}

void Server::do_accept()
{
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            if (check_ec(ec, __func__))
            {
                gl_logger->info("Server accepted connection");

                auto session = std::make_shared<HTTPSession>(io_, std::move(socket));
                session->start();
            }

            if (running_mode_ == ServerRunningMode::Persistent)
                do_accept();
        });
}
