#pragma once

#include "common/ec-handler.h"
#include "utils/http-helper.h"
#include <asio.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/io_context.hpp>
#include <asio/ssl.hpp>
#include <iostream>
#include <sstream>
#include <string>

#include "common/session.h"
#include "utils/session-helper.h"

using asio::ip::tcp;
using session::helper::SessionBase;

template <typename T>
class HTTPSession : public Session,
                    public SessionBase<HTTPSession<T>>,
                    public std::enable_shared_from_this<HTTPSession<T>>
{
    constexpr static size_t buffer_size = 4096;

    using TSession = HTTPSession<T>;

    struct Context
    {
        asio::io_context& io;
        asio::strand<asio::any_io_executor>& strand;
        asio::ssl::context& tls_context;
        const HttpRequest& request;
        const uint64_t& session_id;
    };

    class OutgoingSession : public Session,
                            public std::enable_shared_from_this<OutgoingSession>
    {
        inline static const std::string HOST = "api.binance.com";
        inline static const std::string PORT = "443";

    public:
        OutgoingSession(std::shared_ptr<HTTPSession> outer_session)
            : outer_session_(outer_session), context_(outer_session->get_context()),
              resolver_(context_.io), stream_(context_.io, context_.tls_context) {}
        ~OutgoingSession();

        void start() override;
        void stop() override;
        uint64_t get_id() override
        {                                                                                       
            return context_.session_id;
        }

    protected:
        void on_resolve(const asio::error_code& ec, tcp::resolver::results_type results);
        void on_connect(const asio::error_code& ec);
        void on_handshake(const asio::error_code& ec);
        void on_write(const asio::error_code& ec, std::size_t);
        void on_read(const asio::error_code& ec, std::size_t n);

    private:
        bool init_ssl();
        void connect(const tcp::resolver::results_type& endpoints);
        void send_request();
        void read_response();
        void generate_request();
        void on_failure(const asio::error_code& ec);

    private:
        std::shared_ptr<TSession> outer_session_;
        Context context_;
        asio::ssl::stream<tcp::socket> stream_;
        tcp::resolver resolver_;
        std::array<char, buffer_size> buffer_;
        std::stringstream response_;
        std::string http_request_;
        std::atomic<bool> stopped_ = false;
    };

public:
    HTTPSession(asio::io_context& io_, T&& socket, uint64_t id);
    ~HTTPSession() override;

    void start() override;
    void stop() override;
    uint64_t get_id() override
    {
        return SessionBase<TSession>::get_session_id();
    }

protected:
    Context get_context()
    {
        return {io_, strand_, tls_context_, request_, SessionBase<TSession>::id_};
    }

    void on_connect(const asio::error_code& ec);
    void on_read(const asio::error_code& ec, std::size_t n);
    void on_write(const asio::error_code& ec, std::size_t n);
    void on_request(HttpRequest request);
    void on_outgoing_session_completed(std::string response);
    void on_outgoing_session_failed(const asio::error_code& ec);

private:
    bool init_tls_context();
    void obtain_header();
    void shutdown();

private:
    asio::io_context& io_;
    T http_stream_;
    HttpRequest request_;
    asio::strand<asio::any_io_executor> strand_;
    asio::streambuf buffer_;
    std::string raw_request_;
    std::size_t content_length_ = 0;
    asio::ssl::context tls_context_;
    std::string response_;
    std::weak_ptr<OutgoingSession> outgoing_session_;
    std::once_flag socket_shutdown_flag_;
};