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

using asio::ip::tcp;

class HTTPSession : public std::enable_shared_from_this<HTTPSession>
{
    constexpr static size_t buffer_size = 4096;

    struct Context
    {
        asio::io_context& io;
        asio::strand<asio::any_io_executor>& strand;
        asio::ssl::context& tls_context;
        const HttpRequest& request;
        const uint64_t& session_id;
    };

    class OutgoingSession : public std::enable_shared_from_this<OutgoingSession>
    {
        inline static const std::string HOST = "api.binance.com";
        inline static const std::string PORT = "443";

    public:
        OutgoingSession(std::shared_ptr<HTTPSession> outer_session)
            : outer_session_(outer_session), context_(outer_session->get_context()),
              resolver_(context_.io), stream_(context_.io, context_.tls_context) {}
        ~OutgoingSession();
        void start();

    protected:
        void on_connect();

    private:
        bool init_ssl();
        void connect(const tcp::resolver::results_type& endpoints);
        void send_request();
        void read_response();
        void generate_request();

    private:
        std::shared_ptr<HTTPSession> outer_session_;
        Context context_;
        tcp::resolver resolver_;
        asio::ssl::stream<tcp::socket> stream_;
        std::array<char, buffer_size> buffer_;
        std::stringstream response_;
        std::string http_request_;
    };

public:
    HTTPSession(asio::io_context& io_, asio::ip::tcp::socket&& socket, uint64_t id);
    ~HTTPSession();

    void start();
    uint64_t get_id()
    {
        return id_;
    }

protected:
    Context get_context()
    {
        return {io_, strand_, tls_context_, request_, id_};
    }
    void on_request(HttpRequest request);
    void on_outgoing_session_completed(const asio::error_code& ec, std::string response);

private:
    bool init_tls_context();
    void obtain_header();

private:
    asio::io_context& io_;
    tcp::socket socket_;
    HttpRequest request_;
    asio::strand<asio::any_io_executor> strand_;
    asio::streambuf buffer_;
    std::string raw_request_;
    std::size_t content_length_ = 0;
    asio::ssl::context tls_context_;
    std::string response_;
    uint64_t id_{0};
};