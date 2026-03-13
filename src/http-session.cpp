#include "http-session.h"

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <spdlog/fmt/fmt.h>

#include "logs/logger.h"

template <typename Stream>
auto& lowest_socket(Stream& stream)
{
    if constexpr (std::is_same_v<Stream, tcp::socket>)
        return stream;
    else if constexpr (std::is_same_v<Stream, asio::ssl::stream<tcp::socket>>)
        return stream.lowest_layer();
}

template <typename T>
HTTPSession<T>::HTTPSession(asio::io_context& io, T&& socket, uint64_t id)
    : io_(io), http_stream_(std::move(socket)), strand_(asio::make_strand(http_stream_.get_executor())),
      tls_context_(asio::ssl::context::tls_client),
      id_(id)
{

    gl_logger->trace("HTTPSession constructed, id: {}", id_);
}

template <typename T>
HTTPSession<T>::~HTTPSession()
{
    gl_logger->trace("HTTPSession destructed, id: {}", id_);
}

template <typename T>
void HTTPSession<T>::start()
{
    gl_logger->info("HTTPSession started, id: {} ...", id_);

    asio::co_spawn(strand_, start_impl(), asio::detached);
}

template <typename T>
void HTTPSession<T>::stop()
{
    gl_logger->info("HTTPSession session, stop pending, id: {} ...", id_);
    stopped_ = true;
}

template <typename T>
awaitable<void> HTTPSession<T>::start_impl()
{
    auto self = this->shared_from_this();

    if (!init_tls_context())
    {
        gl_logger->info("HTTPSession failed, id: {} ...", id_);
        co_return;
    }

    co_await obtain_header();
}

template <typename T>
awaitable<void> HTTPSession<T>::obtain_header()
{
    auto [ec, bytes_transferred] =
        co_await asio::async_read_until(http_stream_, buffer_,
                                        http_request_headers_delimiter, asio::as_tuple(use_awaitable));
    if (!check_ec(ec, __func__))
        co_return;

    std::istream stream(&buffer_);
    raw_request_.resize(bytes_transferred);
    stream.read(&raw_request_[0], bytes_transferred);
    HttpRequest request = parse_request(raw_request_);
    on_request(request);
}

template <typename T>
void HTTPSession<T>::on_request(HttpRequest request)
{
    if (stopped_)
        return;

    request_ = std::move(request);

    auto self = this->shared_from_this();
    auto outgoing_session = std::make_shared<HTTPSession::OutgoingSession>(self);

    outgoing_session->start();
}

template <typename T>
bool HTTPSession<T>::init_tls_context()
{
    bool result(true);
    std::string failure_hints;

    try
    {
        // Verify server certificate
        tls_context_.set_verify_mode(asio::ssl::verify_peer);
        tls_context_.set_verify_callback(asio::ssl::host_name_verification(OutgoingSession::HOST));

#ifdef _WIN32
        const std::string cert_path = "certs\\cacert.pem";
        failure_hints = fmt::format("(possibly SSL certificate not found ({})", cert_path);

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
awaitable<void> HTTPSession<T>::on_outgoing_session_completed(const asio::error_code& ec_in, std::string response)
{
    gl_logger->info("OutgoingSession completed, id: {}", id_);
    gl_logger->trace("Response {}", response);

    response_ = std::move(response);

    auto buff = asio::buffer(response_);
    auto [ec, _] = co_await asio::async_write(http_stream_, buff,
                                              asio::as_tuple(use_awaitable));
    check_ec(ec, __func__);

    shutdown();
};

template <typename T>
void HTTPSession<T>::shutdown()
{
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
        co_await resolver_.async_resolve(HOST, PORT, asio::as_tuple(use_awaitable));
    if (check_ec(ec, __func__))
    {
        co_await connect(results);
    }
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::connect(const tcp::resolver::results_type& endpoints)
{
    auto [ec, _] =
        co_await asio::async_connect(stream_.next_layer(), endpoints, asio::as_tuple(use_awaitable));

    if (check_ec(ec, __func__))
    {
        co_await on_connect();
    }
};

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::on_connect()
{
    gl_logger->info("OutgoingSession connected, id: {}", context_.session_id);

    co_await stream_.async_handshake(asio::ssl::stream_base::client, use_awaitable);

    co_await send_request();
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::send_request()
{
    generate_request();

    auto [ec, _] =
        co_await asio::async_write(stream_, asio::buffer(http_request_), asio::as_tuple(use_awaitable));

    if (check_ec(ec, __func__))
    {
        co_await read_response();
    }
}

template <typename T>
awaitable<void> HTTPSession<T>::OutgoingSession::read_response()
{
    auto [ec, n] =
        co_await stream_.async_read_some(asio::buffer(buffer_), asio::as_tuple(use_awaitable));

    if (is_eof(ec))
    {
        std::string resp(response_.str());

        co_await outer_session_->on_outgoing_session_completed(ec, std::move(resp));
    }
    else if (check_ec(ec, __func__))
    {
        response_.write(buffer_.data(), static_cast<std::streamsize>(n));
        co_await read_response();
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
                                context_.request.target, HOST, user_agent);
}

template <typename T>
bool HTTPSession<T>::OutgoingSession::init_ssl()
{
    // SNI (many hosts require it)
    bool result = SSL_set_tlsext_host_name(stream_.native_handle(), HOST.c_str());
    if (!result)
    {
        gl_logger->error("Failed to set SNI host name {}", HOST);
    }
    return result;
}

// explicit specialization:
template class HTTPSession<tcp::socket>;
template class HTTPSession<asio::ssl::stream<tcp::socket>>;