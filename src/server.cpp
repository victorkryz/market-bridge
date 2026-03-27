#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <atomic>
#include <sstream>

#include "server.h"
#include "common/ec-handler.h"
#include "http-session.h"
#include "handshake_session.h"
#include "logs/logger.h"

void ssl_info_callback(const SSL* ssl, int where, int ret);

Server::Server(const Config& cfg) : running_mode_(cfg.running_mode),
                                    signals_(io_, SIGINT, SIGTERM),
                                    ssl_context_(asio::ssl::context::tls_server),
                                    http_port_(cfg.http_port), https_port_(cfg.https_port),
                                    cert_path(cfg.srv_cert_path),
                                    private_key_path(cfg.srv_private_key_path)
{
}

int Server::run()
{
    gl_logger->info("Server running ...");

    init_acceptors();
    install_signals_handler();
    install_listeners();

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
    bool expected(false);
    if (!shutdown_pending_.compare_exchange_strong(expected, true))
        return;

    gl_logger->info("Server, shutdown pending ...");

    stop_sessions();
    close_acceptors();
}

void Server::listener(asio::ip::tcp::acceptor& acceptor,
                      std::function<void(asio::ip::tcp::socket)> completion_handler)
{
    acceptor.async_accept(
        [this, &acceptor, completion_handler](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            if (check_ec(ec, __func__))
            {
                gl_logger->info("Server accepted connection");

                if (completion_handler)
                    std::invoke(completion_handler, std::move(socket));
            }

            if ((running_mode_ == ServerRunningMode::Persistent) && !shutdown_pending_)
                listener(acceptor, completion_handler);
        });
}

void Server::dispatch_request(asio::ip::tcp::socket socket)
{
    // TODO: rewrite this using async approach

    constexpr uint8_t tls_handshake_sign[] = {0x16, 0x03, 0x01};

    std::array<uint8_t, 3> buff;
    asio::error_code ec;
    size_t n = socket.receive(asio::buffer(buff),
                              asio::socket_base::message_peek, ec);

    if (!check_ec(ec, __func__) )
    {
       gl_logger->error("Cannot detect input protocol");
       return; 
    }

    // check if https protocol:
    if ((n >= buff.size()) &&
        (buff[0] == tls_handshake_sign[0] && buff[1] == tls_handshake_sign[1]))
    {
        ssl_handshake(std::move(socket));
    }
    else
    {
        launch_http_session(std::move(socket));
    }
}

void Server::ssl_handshake(asio::ip::tcp::socket&& socket)
{
    std::call_once(ssl_context_init_flag_, [this]
                   { init_ssl_context(); });

    asio::ssl::stream<tcp::socket> ssl_stream(std::move(socket), ssl_context_);
    auto session = std::make_shared<HandshakeSession>(std::move(ssl_stream),
                                                      generate_session_id(),
                                                      [this](asio::ssl::stream<tcp::socket> s)
                                                      {
                                                          on_ssl_handshake_done(std::move(s));
                                                      });
    register_session(session);
    session->start();
}

void Server::on_ssl_handshake_done(asio::ssl::stream<tcp::socket>&& stream)
{
    launch_http_session(std::move(stream));
}

template <typename T>
inline void Server::launch_http_session(T&& channel)
{
    std::shared_ptr<HTTPSession<T>> session = std::make_shared<HTTPSession<T>>(io_, std::move(channel),
                                                                               generate_session_id());
    register_session(session);
    session->start();
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

void Server::init_acceptors()
{
    http_acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_, asio::ip::tcp::endpoint(tcp::v4(), http_port_));
    if (https_port_ != http_port_)
    {
        https_acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_, asio::ip::tcp::endpoint(tcp::v4(), https_port_));
    }
}

void Server::install_listeners()
{
    if (http_acceptor_)
        listener(*http_acceptor_, [this](asio::ip::tcp::socket s)
                 { dispatch_request(std::move(s)); });

    if (https_acceptor_)
        listener(*https_acceptor_,
                 [this](asio::ip::tcp::socket socket)
                 { ssl_handshake(std::move(socket)); });
}

void Server::init_ssl_context()
{
    ssl_context_.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::single_dh_use);

    if (!cert_path.empty())
        ssl_context_.use_certificate_chain_file(cert_path);
    if (!private_key_path.empty())
        ssl_context_.use_private_key_file(private_key_path, asio::ssl::context::pem);

    SSL_CTX_set_info_callback(ssl_context_.native_handle(), ssl_info_callback);
}

void Server::register_session(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lg(session_mtx_);
    sessions_.push_back(session);
}

void Server::stop_sessions()
{
    std::vector<std::shared_ptr<Session>> active_sessions;

    {
        std::lock_guard<std::mutex> lg(session_mtx_);

        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (auto session = it->lock())
            {
                active_sessions.push_back(session);
                ++it;
            }
            else
            {
                it = sessions_.erase(it);
            }
        }
    }

    for (auto session : active_sessions)
        session->stop();
}

void Server::close_acceptors()
{
    if (http_acceptor_)
        http_acceptor_->close();

    if (https_acceptor_)
        https_acceptor_->close();

    http_acceptor_.reset();
    https_acceptor_.reset();
}

void ssl_info_callback(const SSL* ssl, int where, int ret)
{
    const char* state = SSL_state_string_long(ssl);

    if (where & SSL_CB_LOOP)
    {
        gl_logger->trace("[TLS][LOOP] {}", state);
    }
    else if (where & SSL_CB_HANDSHAKE_START)
    {
        gl_logger->trace("TLS] Handshake started");
    }
    else if (where & SSL_CB_HANDSHAKE_DONE)
    {
        gl_logger->trace("[TLS] Handshake done");
    }
    else if (where & SSL_CB_ALERT)
    {
        std::stringstream s;
        s << "[TLS][ALERT] "
          << ((where & SSL_CB_READ) ? "read" : "write")
          << " : "
          << SSL_alert_type_string_long(ret)
          << " : "
          << SSL_alert_desc_string_long(ret)
          << std::endl;
        gl_logger->trace(s.str());
    }
    else if (where & SSL_CB_EXIT)
    {
        if (ret == 0)
        {
            gl_logger->trace("[TLS][EXIT] failed in state: {}", state);
        }
        else if (ret < 0)
        {
            gl_logger->trace("[TLS][EXIT] error in state: {}", state);
        }
    }
}