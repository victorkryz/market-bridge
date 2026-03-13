#pragma once

#include <asio.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/io_context.hpp>
#include <asio/ssl.hpp>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

#include "common/ec-handler.h"
#include "utils/http-helper.h"
#include <string>

#include "common/session.h"

using asio::ip::tcp;

using asio::awaitable;
using asio::use_awaitable;
namespace this_coro = asio::this_coro;

template <typename T>
class HTTPSession : public Session,
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
    public:
        inline static const std::string HOST = "api.binance.com";
        inline static const std::string PORT = "443";

    public:
        OutgoingSession(std::shared_ptr<HTTPSession> outer_session)
            : outer_session_(outer_session), context_(outer_session->get_context()),
              resolver_(context_.io), stream_(context_.io, context_.tls_context) {}
        ~OutgoingSession();

        void start() override;
        void stop() override {};
        uint64_t get_id() override
        {
            return context_.session_id;
        }

    protected:
        awaitable<void> start_impl();
        awaitable<void> on_connect();

    private:
        awaitable<void> connect(const tcp::resolver::results_type& endpoints);
        awaitable<void> send_request();
        awaitable<void> read_response();
        bool init_ssl();
        void generate_request();

    private:
        std::shared_ptr<TSession> outer_session_;
        Context context_;
        tcp::resolver resolver_;
        asio::ssl::stream<tcp::socket> stream_;
        std::array<char, buffer_size> buffer_;
        std::stringstream response_;
        std::string http_request_;
    };

public:
    HTTPSession(asio::io_context& io_, T&& socket, uint64_t id);
    ~HTTPSession() override;

    void start() override;
    void stop() override;
    uint64_t get_id() override
    {
        return id_;
    }

protected:
    awaitable<void> start_impl();
    Context get_context()
    {
        return {io_, strand_, tls_context_, request_, id_};
    }
    void on_request(HttpRequest request);
    awaitable<void> on_outgoing_session_completed(const asio::error_code& ec, std::string response);
    void shutdown();

private:
    bool init_tls_context();
    awaitable<void> obtain_header();

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
    uint64_t id_{0};
    bool stopped_ = false;
};