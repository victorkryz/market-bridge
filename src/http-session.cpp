#include "http-session.h"

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <spdlog/fmt/fmt.h>
#include <type_traits>

#include "utils/session-helper.h"
#include "logs/logger.h"

template <typename T>
HTTPSession<T>::HTTPSession(const Config& cfg,
                            asio::io_context& io,
                            T&& socket, uint64_t id) : SessionBase<HTTPSession<T>>(id),
                                                       io_(io), http_stream_(std::move(socket)),
                                                       strand_(asio::make_strand(http_stream_.get_executor())),
                                                       tls_context_(asio::ssl::context::tls_client),
                                                       upstream_info_{cfg.upstream_host, cfg.upstream_port, cfg.ignore_certificate_verification}

{

    gl_logger->trace("HTTPSession constructed, id: {}", this->get_session_id());
}

template <typename T>
HTTPSession<T>::~HTTPSession()
{
    gl_logger->trace("HTTPSession destructed, id: {}", this->get_session_id());
}

template <typename T>
void HTTPSession<T>::start()
{
    gl_logger->info("HTTPSession started, id: {} ...", this->get_session_id());

    if (!init_tls_context())
    {
        gl_logger->info("HTTPSession TLS init failed, id: {} ...", this->get_session_id());
        stop();
    }
    else
    {
        obtain_header();
    }
}

template <typename T>
void HTTPSession<T>::stop()
{
    auto self = this->shared_from_this();
    asio::dispatch(strand_, [this, self]()
                   { 
                        if (self->request_stop())
                        {  
                            gl_logger->info("HTTPSession session, stop pending, id: {} ...",  this->get_session_id());
                            self->shutdown(); 
                        } });
}

template <typename T>
bool HTTPSession<T>::init_tls_context()
{
    bool result(true);
    std::string failure_hints;

    try
    {
#ifdef _WIN32
        const std::string cert_path = "cert\\cacert.pem";
        failure_hints = fmt::format("(possibly SSL certificate not found ({})", cert_path);

        tls_context_.set_verify_mode(asio::ssl::verify_peer);
        tls_context_.load_verify_file(cert_path);
#else
        // Use system CA certificates (Linux/macOS typically OK)
        tls_context_.set_default_verify_paths();
#endif
    }
    catch (const std::exception& e)
    {
        gl_logger->error("TLS initialization failure {} {}", e.what(), failure_hints);
        result = false;
    }

    return result;
}

template <typename T>
void HTTPSession<T>::obtain_header()
{
    std::shared_ptr<HTTPSession<T>> self = this->shared_from_this();

    asio::async_read_until(http_stream_, buffer_,
                           http_request_headers_delimiter,
                           asio::bind_executor(strand_,
                                               [this, self](const asio::error_code& ec,
                                                            std::size_t bytes_transferred)
                                               {
                                                   on_read(ec, bytes_transferred);
                                               }));
}

template <typename T>
void HTTPSession<T>::on_read(const asio::error_code& ec, std::size_t bytes_transferred)
{
    if (!check_ec(ec, __func__))
    {
        stop();
        return;
    }

    std::istream stream(&buffer_);
    raw_request_.resize(bytes_transferred);
    stream.read(&raw_request_[0], bytes_transferred);

    HttpRequest request = parse_request(raw_request_);
    on_request(request);
}

template <typename T>
void HTTPSession<T>::on_request(HttpRequest request)
{
    if (this->is_stopped())
        return;

    auto self = this->shared_from_this();
    request_ = std::move(request);

    auto outgoing_session = std::make_shared<HTTPSession<T>::OutgoingSession>(self);
    outgoing_session_ = outgoing_session;
    outgoing_session->start();
}

template <typename T>
void HTTPSession<T>::on_outgoing_session_completed(std::string response)
{
    gl_logger->info("OutgoingSession completed, id: {}", this->get_session_id());
    gl_logger->trace("Response {}", response);

    response_ = std::move(response);

    auto self = this->shared_from_this();

    auto buff = asio::buffer(response_);
    asio::async_write(
        http_stream_, buff,
        asio::bind_executor(strand_,
                            [this, self](const asio::error_code& ec, std::size_t n)
                            {
                                on_write(ec, n);
                            }));
};

template <typename T>
void HTTPSession<T>::on_outgoing_session_failed(const asio::error_code& ec)
{
    if (!this->request_stop())
        return;

    gl_logger->info("HTTPSession id: {}: OutgoingSession failed, reason: {}, error code: {}",
                    this->get_session_id(), ec.message(), ec.value());

    static constexpr std::string_view response_body = "Bad Gateway";

    response_ = fmt::format(
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: {}\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}",
        response_body.size(),
        response_body);

    auto buff = asio::buffer(response_);

    auto self = this->shared_from_this();

    asio::async_write(http_stream_, buff,
                      asio::bind_executor(
                          strand_,
                          [self, this, response = response_body](const asio::error_code& ec, std::size_t)
                          {
                              if (check_ec(ec, __func__))
                              {
                                  gl_logger->info("HTTPSession id: {}, response sent: {}",
                                                  this->get_session_id(), response);
                              }

                              gl_logger->info("HTTPSession id: {}, shutting down...",
                                              this->get_session_id());

                              // even if write fails — just shutdown
                              self->shutdown();
                          }));
}

template <typename T>
void HTTPSession<T>::on_write(const asio::error_code& ec, std::size_t n)
{
    // log only, cleanup follows regardless
    check_ec(ec, __func__);

    shutdown();
}

template <typename T>
void HTTPSession<T>::shutdown()
{
    using namespace session::helper;

    std::call_once(socket_shutdown_flag_, [this]
                   {
                    auto& socket = lowest_socket<T>(http_stream_);
                    shutdown_socket(socket); });

    if (auto outgoing_session = outgoing_session_.lock())
        outgoing_session->stop();
}

template <typename T>
HTTPSession<T>::OutgoingSession::~OutgoingSession()
{
    gl_logger->trace("OutgoingSession destructed id: {}...", context_.session_id);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::start()
{
    auto self = this->shared_from_this();

    if (!init_ssl())
    {
        gl_logger->error("SSL initialization failure!");
        asio::post(context_.strand, [this, self]()
                   { on_failure(asio::error::operation_aborted); });
        return;
    }

    gl_logger->info("OutgoingSession started, id: {}", context_.session_id);

    resolver_.async_resolve(context_.upstream_info.host, std::to_string(context_.upstream_info.port),
                            asio::bind_executor(context_.strand,
                                                [this, self](const asio::error_code& ec,
                                                             tcp::resolver::results_type results)
                                                {
                                                    on_resolve(ec, results);
                                                }));
}

template <typename T>
void HTTPSession<T>::OutgoingSession::stop()
{
    auto self = this->shared_from_this();
    asio::dispatch(context_.strand, [this, self]
                   {
                        if (!stopped_.exchange(true))
                        {   
                            gl_logger->info("Outgoing session, stop pending, id: {} ...", context_.session_id);

                            using namespace session::helper;
                            shutdown_socket(lowest_socket(stream_));
                        } });
}

template <typename T>
void HTTPSession<T>::OutgoingSession::connect(const tcp::resolver::results_type& endpoints)
{
    auto self = this->shared_from_this();

    asio::async_connect(stream_.next_layer(), endpoints,
                        asio::bind_executor(context_.strand,
                                            [this, self](const asio::error_code& ec, const tcp::endpoint&)
                                            {
                                                on_connect(ec);
                                            }));
};

template <typename T>
void HTTPSession<T>::OutgoingSession::on_resolve(const asio::error_code& ec,
                                                 tcp::resolver::results_type results)
{
    if (check_ec(ec, __func__))
        connect(results);
    else
        on_failure(ec);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::on_connect(const asio::error_code& ec)
{
    if (!check_ec(ec, __func__))
    {
        on_failure(ec);
    }
    else
    {

        gl_logger->info("OutgoingSession connected, id: {}", context_.session_id);

        auto self = this->shared_from_this();

        stream_.async_handshake(asio::ssl::stream_base::client,
                                asio::bind_executor(context_.strand,
                                                    [this, self](const asio::error_code& ec)
                                                    {
                                                        on_handshake(ec);
                                                    }));
    }
}

template <typename T>
void HTTPSession<T>::OutgoingSession::on_handshake(const asio::error_code& ec)
{
    if (check_ec(ec, __func__))
        send_request();
    else
        on_failure(ec);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::send_request()
{
    auto self = this->shared_from_this();

    generate_request();

    asio::async_write(stream_, asio::buffer(http_request_),
                      asio::bind_executor(context_.strand,
                                          [this, self](const asio::error_code& ec, std::size_t sz)
                                          {
                                              on_write(ec, sz);
                                          }));
}

template <typename T>
void HTTPSession<T>::OutgoingSession::on_write(const asio::error_code& ec, std::size_t)
{
    if (check_ec(ec, __func__))
        read_response();
    else
        on_failure(ec);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::read_response()
{
    auto self = this->shared_from_this();

    stream_.async_read_some(asio::buffer(buffer_),
                            asio::bind_executor(context_.strand,
                                                [this, self](const asio::error_code& ec, std::size_t n)
                                                {
                                                    on_read(ec, n);
                                                }));
}

template <typename T>
void HTTPSession<T>::OutgoingSession::on_read(const asio::error_code& ec, std::size_t n)
{
    if (is_eof(ec))
    {
        std::string resp(response_.str());
        outer_session_->on_outgoing_session_completed(std::move(resp));
    }
    else if (check_ec(ec, __func__))
    {
        response_.write(buffer_.data(), static_cast<std::streamsize>(n));
        read_response(); // continue reading
    }
    else
    {
        on_failure(ec);
    }
}

template <typename T>
void HTTPSession<T>::OutgoingSession::on_failure(const asio::error_code& ec)
{
    if (outer_session_)
        outer_session_->on_outgoing_session_failed(ec);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::generate_request()
{
    std::string user_agent;
    auto it = context_.request.headers.find("User-Agent");
    if (it != context_.request.headers.end())
        user_agent = it->second;
    else
        user_agent = "market-bridge/1.0.0";

    http_request_ = fmt::format("GET {} "
                                "HTTP/1.1\r\n"
                                "Host: {}\r\n"
                                "User-Agent: {}\r\n"
                                "Accept: */*\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                context_.request.target, context_.upstream_info.host, user_agent);
}

template <typename T>
bool HTTPSession<T>::OutgoingSession::init_ssl()
{
    const auto& upstream_info = context_.upstream_info;

    if (upstream_info.ignore_certificate_verification)
    {
        gl_logger->warn("Certificate verification disabled for host {}", upstream_info.host);
        stream_.set_verify_mode(asio::ssl::verify_none);
    }
    else
    {
        // Verify server certificate (important)
        stream_.set_verify_mode(asio::ssl::verify_peer);
        stream_.set_verify_callback(asio::ssl::host_name_verification(upstream_info.host));
    }

    // SNI (many hosts require it)
    bool result = SSL_set_tlsext_host_name(stream_.native_handle(), upstream_info.host.c_str());
    if (!result)
    {
        gl_logger->error("Failed to set SNI host name {}", upstream_info.host);
    }
    return result;
}

// explicit instantiation:
template class HTTPSession<tcp::socket>;
template class HTTPSession<asio::ssl::stream<tcp::socket>>;