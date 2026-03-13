#include "common/ec-handler.h"
#include "logs/logger.h"
#include "handshake_session.h"

void log_openssl_errors();

HandshakeSession::HandshakeSession(HandshakeSession::ssl_stream&& stream,
                                   asio::ssl::context& ssl_context, uint64_t id,
                                   std::function<void(ssl_stream)> handshake_completion_notifier) : stream_(std::move(stream)),
                                                                                                    ssl_context_(ssl_context),
                                                                                                    id_(id),
                                                                                                    handshake_completion_notifier_(std::move(handshake_completion_notifier))
{
}

HandshakeSession::~HandshakeSession()
{
    gl_logger->info("Handshake session finished!");
}

void HandshakeSession::start()
{
    gl_logger->info("Handshake session started!");

    auto strand = asio::make_strand(stream_.get_executor());
    asio::co_spawn(strand, start_impl(), asio::detached);
}

awaitable<void> HandshakeSession::start_impl()
{
    auto self = this->shared_from_this();

    auto [ec] =
        co_await stream_.async_handshake(asio::ssl::stream_base::server, asio::as_tuple(use_awaitable));

    if (check_ec(ec, __func__))
    {
        gl_logger->info("Handshake succeeded!");

        if (handshake_completion_notifier_)
            std::invoke(handshake_completion_notifier_, std::move(stream_));
    }
    else
    {
        log_openssl_errors();
    }
}

void log_openssl_errors()
{
    unsigned long err = 0;
    while ((err = ERR_get_error()) != 0)
    {
        char buffer[256]{};
        ERR_error_string_n(err, buffer, sizeof(buffer));
        gl_logger->error("OpenSSL: {}", buffer);
    }
}
