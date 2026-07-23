#include "http-session.h"
#include <string_view>

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <spdlog/fmt/fmt.h>

#include "utils/session-helper.h"
#include "logs/logger.h"

using namespace std::literals;

template <typename T>
HTTPSession<T>::HTTPSession(const Config& cfg,
                            asio::io_context& io,
                            T&& socket, uint64_t id) : SessionBase<HTTPSession<T>>(id),
                                                       io_(io), http_stream_(std::move(socket)),
                                                       strand_(asio::make_strand(http_stream_.get_executor())),
                                                       tls_context_(asio::ssl::context::tls_client),
                                                       upstream_info_{
                                                           .host = cfg.upstream_host,
                                                           .port = cfg.upstream_port,
                                                           .ignore_certificate_verification =
                                                               cfg.ignore_certificate_verification}
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

    asio::co_spawn(strand_, start_impl(), asio::detached);
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
awaitable<void> HTTPSession<T>::start_impl()
{
    auto self = this->shared_from_this();

    if (!init_tls_context())
    {
        gl_logger->info("HTTPSession failed, id: {} ...", this->get_session_id());
        co_return;
    }

    co_await obtain_header();
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
awaitable<void> HTTPSession<T>::obtain_header()
{
    std::shared_ptr<HTTPSession<T>> self = this->shared_from_this();

    auto reading_timeout_guard = std::make_shared<asio::steady_timer>(strand_, std::chrono::seconds(5));
    reading_timeout_guard->async_wait(asio::bind_executor(strand_,
                                                          [this, self,
                                                           reading_timeout_guard](const asio::error_code& ec)
                                                          {
                                                              if (!is_aborted(ec))
                                                              {
                                                                  co_spawn(strand_, on_header_timeout(ec), asio::detached);
                                                              }
                                                          }));

    auto [ec, bytes_transferred] =
        co_await asio::async_read_until(http_stream_, buffer_,
                                        http_request_headers_delimiter, asio::as_tuple(use_awaitable));
    if (!check_ec(ec))
        co_return;

    reading_timeout_guard->cancel();
    on_header_obtained(ec, bytes_transferred);
}

template <typename T>
void HTTPSession<T>::on_header_obtained(asio::error_code ec, std::size_t bytes_transferred)
{
    if (is_aborted(ec) ||
        this->is_stopped())
        return;

    if (!check_ec(ec))
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
awaitable<void> HTTPSession<T>::on_header_timeout(asio::error_code ec)
{
    if (is_cancelled(ec))
        co_return; // timeout cancelled because header was obtained

    if (!check_ec(ec))
    {
        stop();
        co_return;
    }

    if (!this->request_stop())
        co_return;

    static constexpr std::string_view response_body = "Request Timeout";

    response_ = std::move(generate_error_response(HTTPResponseCodes::RequestTimeout,
                                                  response_body, std::string(response_body)));
    auto buff = asio::buffer(response_);

    auto self = this->shared_from_this();

    auto [write_ec, _] = co_await asio::async_write(http_stream_, buff,
                                                    asio::as_tuple(use_awaitable));

    if (check_ec(write_ec))
    {
        gl_logger->trace("HTTPSession id: {}, response sent: {}",
                        this->get_session_id(), response_);
    }

    gl_logger->info("HTTPSession id: {}, header read timeout, shutting down...", this->get_session_id());
    self->shutdown();
}

template <typename T>
void HTTPSession<T>::on_request(HttpRequest request)
{
    if (this->is_stopped())
        return;

    gl_logger->trace("Request: {}", request.to_string());

    if (request.target == "/health")
    {
        asio::co_spawn(strand_, on_health_request(std::move(request)), asio::detached);
    }
    else
    {
        request_ = std::move(request);

        auto self = this->shared_from_this();
        auto outgoing_session = std::make_shared<HTTPSession::OutgoingSession>(self);

        outgoing_session->start();
    }
}

template <typename T>
awaitable<void> HTTPSession<T>::on_health_request(HttpRequest request)
{
    constexpr std::string_view response_body_template = R"(
                                    {{
                                        "service": "{}",
                                        "status": "{}",
                                        "version": "{}" 
                                    }})"sv;
    HttpResponse response{
        .status_code = static_cast<int>(HTTPResponseCodes::OK),
        .reason = "OK",
        .body = fmt::format(response_body_template, APP_NAME, "UP", APP_VERSION)};

    response.headers["Content-Type"] = "application/json";
    response.headers["Content-Length"] = std::to_string(response.body.size());
    response.headers["Connection"] = "close";

    response_ = std::move(response.to_string());
    auto buff = asio::buffer(response_);

    auto self = this->shared_from_this();

    auto [ec, _] = co_await asio::async_write(http_stream_, buff,
                                              asio::as_tuple(use_awaitable));

    if (check_ec(ec))
    {
        gl_logger->trace("HTTPSession id: {}, response sent: {}",
                        this->get_session_id(), response_);
    }
}

template <typename T>
awaitable<void> HTTPSession<T>::on_outgoing_session_completed(asio::error_code ec_in, std::string response)
{
    gl_logger->info("OutgoingSession completed, id: {}", this->get_session_id());
    gl_logger->trace("Response {}", response);

    auto self = this->shared_from_this();

    response_ = std::move(response);

    auto buff = asio::buffer(response_);
    auto [ec, _] = co_await asio::async_write(http_stream_, buff,
                                              asio::as_tuple(use_awaitable));
    check_ec(ec);

    shutdown();
};

template <typename T>
void HTTPSession<T>::shutdown()
{
    using namespace session::helper;

    auto& socket = lowest_socket<T>(http_stream_);

    asio::error_code ec_formal;
    auto rc = socket.shutdown(tcp::socket::shutdown_both, ec_formal);
    socket.close();
}

template <typename T>
HTTPSession<T>::OutgoingSession::~OutgoingSession()
{
    gl_logger->trace("OutgoingSession destructed, id: {}...", context_.session_id);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::start()
{
    gl_logger->info("OutgoingSession started, id: {}", context_.session_id);

    asio::co_spawn(context_.strand, start_impl(), asio::detached);
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::start_impl()
{
    if (!init_ssl())
    {
        gl_logger->error("SSL initialization failure!");
        co_return;
    }

    auto self = this->shared_from_this();

    auto [ec, results] =
        co_await resolver_.async_resolve(context_.upstream_info.host, std::to_string(context_.upstream_info.port),
                                         asio::as_tuple(use_awaitable));
    if (check_ec(ec, __func__))
    {
        co_await connect(results);
    }
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::connect(const tcp::resolver::results_type& endpoints)
{
    auto self = this->shared_from_this();

    auto [ec, _] =
        co_await asio::async_connect(stream_.next_layer(), endpoints, asio::as_tuple(use_awaitable));

    if (check_ec(ec))
    {
        co_await on_connect();
    }
};

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::on_connect()
{
    gl_logger->info("OutgoingSession connected, id: {}", context_.session_id);

    auto self = this->shared_from_this();
    co_await stream_.async_handshake(asio::ssl::stream_base::client, use_awaitable);

    co_await send_request();
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::send_request()
{
    generate_request();

    auto self = this->shared_from_this();

    auto [ec, _] =
        co_await asio::async_write(stream_, asio::buffer(http_request_), asio::as_tuple(use_awaitable));

    if (check_ec(ec))
    {
        co_await read_response();
    }
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::read_response()
{
    auto self = this->shared_from_this();

    for (;;)
    {
        auto [ec, n] = co_await stream_.async_read_some(
            asio::buffer(buffer_),
            asio::as_tuple(use_awaitable));
        if (is_eof(ec))
        {
            std::string resp(response_.str());
            co_await outer_session_->on_outgoing_session_completed(ec, std::move(resp));
            co_return;
        }

        if (!check_ec(ec))
            co_return;

        response_.write(buffer_.data(), static_cast<std::streamsize>(n));
    }
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

// explicit specialization:
template class HTTPSession<tcp::socket>;
template class HTTPSession<asio::ssl::stream<tcp::socket>>;