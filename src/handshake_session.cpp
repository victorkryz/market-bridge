#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common/ec-handler.h"

#include "handshake_session.h"

#include "utils/session-helper.h"
#include "logs/logger.h"

void log_openssl_errors();

HandshakeSession::HandshakeSession(HandshakeSession::ssl_stream&& stream, uint64_t id,
                                   std::function<void(ssl_stream)> handshake_completion_notifier) : SessionBase(id),
                                                                                                    stream_(std::move(stream)),
                                                                                                    handshake_completion_notifier_(std::move(handshake_completion_notifier))
{
}

HandshakeSession::~HandshakeSession()
{
    gl_logger->info("Handshake session finished, id: {}", get_session_id());
}

void HandshakeSession::start()
{
    gl_logger->info("Handshake session started, id: {}", get_session_id());

    handshake_impl();
}

void HandshakeSession::stop()
{
    auto self = this->shared_from_this();

    asio::dispatch(stream_.get_executor(), [this, self]()
                   {
                        if ( request_stop() )
                        {
                            gl_logger->info("HandshakeSession session, stop pending, id: {} ...",  get_session_id());
                            
                            using namespace session::helper;    
                            shutdown_socket(lowest_socket(stream_));
                        } });
}

void HandshakeSession::handshake_impl()
{
    auto self = this->shared_from_this();

    stream_.async_handshake(
        asio::ssl::stream_base::server,
        [this, self](const asio::error_code& ec)
        {
            if (check_ec(ec, __func__))
            {
                gl_logger->info("Handshake succeeded, id: {}", get_session_id());

                if (handshake_completion_notifier_)
                    std::invoke(handshake_completion_notifier_, std::move(stream_));
            }
            else
            {
                log_openssl_errors();
            }
        });
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
