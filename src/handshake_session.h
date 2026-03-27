#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common/session.h"
#include "utils/session-helper.h"

using asio::ip::tcp;
using session::helper::SessionBase;

class HandshakeSession : public Session,
                         public SessionBase<HandshakeSession>,
                         public std::enable_shared_from_this<HandshakeSession>
{
    using  ssl_stream = asio::ssl::stream<tcp::socket>;

    public:
        HandshakeSession(ssl_stream&& stream, uint64_t id,
                        std::function<void (ssl_stream)> handshake_completion_notifier);
        ~HandshakeSession() override;

        void start() override;
        void stop() override;
        uint64_t get_id() override { return get_session_id();}

    protected:
        void handshake_impl();

    private:
        asio::ssl::stream<tcp::socket> stream_;
        std::function<void (ssl_stream)> handshake_completion_notifier_;
};
