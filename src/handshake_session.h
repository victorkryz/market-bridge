#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common/session.h"

using asio::ip::tcp;

class HandshakeSession : public Session,
                         public std::enable_shared_from_this<HandshakeSession>
{
    using  ssl_stream = asio::ssl::stream<tcp::socket>;

    public:
        HandshakeSession(ssl_stream&& stream, asio::ssl::context& ssl_context, uint64_t id,
                        std::function<void (ssl_stream)> handshake_completion_notifier);
        ~HandshakeSession() override;

        void start() override;
        void stop() override {};
        uint64_t get_id() override {return id_;}

    protected:
        void handshake_impl();

    private:
        asio::ssl::stream<tcp::socket> stream_;
        asio::ssl::context& ssl_context_;
        uint64_t id_;
        std::function<void (ssl_stream)> handshake_completion_notifier_;
};
