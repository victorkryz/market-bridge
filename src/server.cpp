#include <algorithm>

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
extern std::string make_startup_table(const Config& cfg);

Server::Server(const Config& cfg) : running_mode_(cfg.server.run_mode),
                                    signals_(io_, SIGINT, SIGTERM),
                                    ssl_context_(asio::ssl::context::tls_server),
                                    cfg_(cfg)

{
}

int Server::run()
{
    gl_logger->info("\n{}", make_startup_table(cfg_));
    gl_logger->info("Server running ...");

    install_signals_handler();
    init_acceptors();
    install_listeners();

    const auto num_threads = std::clamp<size_t>(cfg_.server.worker_threads, 
                                                1, std::thread::hardware_concurrency());

    gl_logger->info("Server running with {} threads", num_threads);

    {
        std::vector<std::jthread> threads;
        threads.reserve(num_threads);

        for (size_t i = 0; i < num_threads; i++)
            threads.emplace_back([this]()
                                { io_.run(); });
    }

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

awaitable<void> Server::listener(asio::ip::tcp::acceptor& acceptor,
                                 std::function<void(asio::ip::tcp::socket)> completion_handler)
{
    while (!shutdown_pending_)
    {
        auto [ec, socket] =
            co_await acceptor.async_accept(io_, asio::as_tuple(asio::use_awaitable));

        // Explicit for aborted state (e.g. acceptor closed)

        if (is_aborted(ec))
            co_return;

        if (!check_ec(ec))
            break;

        gl_logger->info("Server accepted connection");

        if (completion_handler)
            completion_handler(std::move(socket));

        if (running_mode_ == ServerRunningMode::SingleRequest)
            break;
    }

    gl_logger->info("Server stopped accepting new connections");

    uninstall_signals_handler();
    uninstall_listeners();
}

asio::awaitable<void> Server::dispatch_http_request(asio::ip::tcp::socket socket)
{
    using std::literals::string_view_literals::operator""sv;

    if (!cfg_.server.allow_https_over_http_port)
    {
        launch_http_session(std::move(socket));
        co_return;
    }

    static constexpr std::array<uint8_t, 2> tls_handshake_sign = {0x16, 0x03};

    std::array<uint8_t, tls_handshake_sign.size()> buff;
    auto [ec, n] = co_await socket.async_receive(asio::buffer(buff),
                                                 asio::socket_base::message_peek,
                                                 asio::as_tuple(asio::use_awaitable));
    if (!check_ec(ec))
    {
        gl_logger->error("Cannot detect input protocol");
        co_return;
    }

    const bool is_tls =
        n >= tls_handshake_sign.size() &&
        std::ranges::equal(buff, tls_handshake_sign);

    if (is_tls)
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
                                                      ssl_context_,
                                                      generate_session_id(),
                                                      [this](asio::ssl::stream<tcp::socket> s)
                                                      {
                                                          on_ssl_handshake_done(std::move(s));
                                                      });
    if (register_session(session))
        session->start();
}

void Server::on_ssl_handshake_done(asio::ssl::stream<tcp::socket>&& stream)
{
    gl_logger->info("SSL handshake completed successfully");
    launch_http_session(std::move(stream));
}

template <session::helper::HttpStream T>
inline void Server::launch_http_session(T&& channel)
{
    std::shared_ptr<HTTPSession<T>> session = std::make_shared<HTTPSession<T>>(cfg_, io_,
                                                                               std::move(channel),
                                                                               generate_session_id());
    if (register_session(session))
        session->start();
}

void Server::init_acceptors()
{
    http_acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_, asio::ip::tcp::endpoint(tcp::v4(), cfg_.server.http_port));
    if (cfg_.server.https_port != cfg_.server.http_port)
    {
        https_acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_, asio::ip::tcp::endpoint(tcp::v4(), cfg_.server.https_port));
    }
}

void Server::install_listeners()
{
    if (http_acceptor_)
        asio::co_spawn(http_acceptor_->get_executor(), listener(*http_acceptor_, [this](asio::ip::tcp::socket s)
                                                                { 
                                        auto executor = s.get_executor();
                                        asio::co_spawn(executor, 
                                            dispatch_http_request(std::move(s)), asio::detached); }),
                       asio::detached);

    if (https_acceptor_)
        asio::co_spawn(https_acceptor_->get_executor(), listener(*https_acceptor_, [this](asio::ip::tcp::socket socket)
                                                                 { ssl_handshake(std::move(socket)); }),
                       asio::detached);
}

void Server::uninstall_listeners()
{
    if (http_acceptor_ && http_acceptor_->is_open())
        http_acceptor_->close();

    if (https_acceptor_ && https_acceptor_->is_open())
        https_acceptor_->close();
}

void Server::install_signals_handler()
{
    signals_.async_wait(
        [this](const asio::error_code& ec, int signo)
        {
            if (check_ec(ec))
            {
                schedule_shutdown();
            }
        });
}

void Server::uninstall_signals_handler()
{
    signals_.cancel();
}

void Server::init_ssl_context()
{
    ssl_context_.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::single_dh_use);

    if (!cfg_.tls.certificate.empty())
        ssl_context_.use_certificate_chain_file(cfg_.tls.certificate);
    if (!cfg_.tls.private_key.empty())
        ssl_context_.use_private_key_file(cfg_.tls.private_key, asio::ssl::context::pem);

    SSL_CTX_set_info_callback(ssl_context_.native_handle(), ssl_info_callback);
}

bool Server::register_session(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lg(session_mtx_);

    bool shutdown = shutdown_pending_.load(std::memory_order_acquire);
    if (!shutdown)
        sessions_.push_back(session);
    else
        gl_logger->trace("Server is shutting down, cannot register new session");

    return !shutdown;
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
    std::lock_guard<std::mutex> lg(acceptor_mtx_);

    for (auto& acceptor : {std::move(http_acceptor_), std::move(https_acceptor_)})
    {
        if (acceptor)
            acceptor->close();
    }
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
        gl_logger->trace("[TLS] Handshake started");
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