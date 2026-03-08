#include "server.h"
#include "common/ec-handler.h"
#include "http-session.h"
#include "logs/logger.h"

Server::Server(unsigned short port, ServerRunningMode running_mode) : running_mode_(running_mode),
                                                                      signals_(io_, SIGINT, SIGTERM),
                                                                      acceptor_(io_, asio::ip::tcp::endpoint(tcp::v4(),
                                                                                                             port)) {}

int Server::run()
{
    gl_logger->info("Server running ...");

    install_signals_handler();

    listener();

    std::vector<std::thread> threads;

    for (size_t i = 0; i < 3; i++)
        threads.emplace_back([this]()
                             { io_.run(); });
    for (auto& th : threads)
        th.join();

    gl_logger->info("Server finished");

    return 0;
}

void Server::schedule_shutdown()
{
    gl_logger->info("Shutdown pending ...");

    shutdown_pending_ = true;
    acceptor_.close();
}

void Server::listener()
{
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            if (check_ec(ec, __func__))
            {
                gl_logger->info("Server accepted connection");

                dispatch_request(std::move(socket));
            }

            if ((running_mode_ == ServerRunningMode::Persistent) && !shutdown_pending_)
                listener();
        });
}

void Server::dispatch_request(asio::ip::tcp::socket socket)
{
    constexpr uint8_t tls_handshake_sign[] = {0x16, 0x03, 0x01};

    std::array<uint8_t, 3> buff;
    size_t n = socket.receive(asio::buffer(buff),
                              asio::socket_base::message_peek);
    // check if https protocol:
    if ((n >= buff.size()) &&
        (buff[0] == tls_handshake_sign[0] && buff[1] == tls_handshake_sign[1]))
    {
        // preventing from access via https link
        // e.g.  https://localhost:8080/api/v3/time
        gl_logger->error("Refused! (HTTPS protocol is currently not supported)");

        asio::error_code ec_formal;
        auto rc = socket.shutdown(tcp::socket::shutdown_both, ec_formal);
        socket.close();
    }
    else
    {
        auto session = std::make_shared<HTTPSession>(io_, std::move(socket),
                                                     generate_session_id());
        session->start();
    }
}

void Server::install_signals_handler()
{
    signals_.async_wait(
        [this](const asio::error_code& ec, int signo)
        {
            if (check_ec(ec, __func__))
            {
                schedule_shutdown();
            }
        });
}