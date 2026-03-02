#include "http-session.h"

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <spdlog/fmt/fmt.h>

#include "logs/logger.h"

HTTPSession::HTTPSession(asio::io_context& io, tcp::socket&& socket, uint64_t id)
    : io_(io), socket_(std::move(socket)), strand_(asio::make_strand(socket_.get_executor())),
      tls_context_(asio::ssl::context::tls_client),
      id_(id)
{
#ifdef _WIN32
    tls_context_.set_verify_mode(asio::ssl::verify_peer);
    tls_context_.load_verify_file("certs\\cacert.pem");
#else
    // Use system CA certificates (Linux/macOS typically OK)
    tls_context_.set_default_verify_paths();
#endif

    gl_logger->trace("HTTPSession constructed, id: {}", id_);
}

HTTPSession::~HTTPSession()
{
    gl_logger->trace("HTTPSession destructed, id: {}", id_);
}

void HTTPSession::start()
{
    gl_logger->info("HTTPSession started, id: {} ...", id_);

    asio::co_spawn(strand_, start_impl(), asio::detached);
}

awaitable<void> HTTPSession::start_impl()
{
    auto self = shared_from_this();
    co_await obtain_header();
}

awaitable<void> HTTPSession::obtain_header()
{
    auto [ec, bytes_transferred] =
        co_await asio::async_read_until(socket_, buffer_,
                                        http_request_headers_delimiter, asio::as_tuple(use_awaitable));
    if (!check_ec(ec, __func__))
        co_return;

    std::istream stream(&buffer_);
    raw_request_.resize(bytes_transferred);
    stream.read(&raw_request_[0], bytes_transferred);
    HttpRequest request = parse_request(raw_request_);
    on_request(request);
}

void HTTPSession::on_request(HttpRequest request)
{
    request_ = std::move(request);

    auto self = shared_from_this();
    auto outgoing_session = std::make_shared<HTTPSession::OutgoingSession>(self);

    asio::co_spawn(strand_, outgoing_session->start(), asio::detached);
}

awaitable<void> HTTPSession::on_outgoing_session_completed(const asio::error_code& ec_in, std::string response)
{
    gl_logger->info("OutgoingSession completed, id: {}", id_);
    gl_logger->trace("Response {}", response);

    response_ = std::move(response);

    auto buff = asio::buffer(response_);
    auto [ec, _] = co_await asio::async_write(socket_, buff,
                                              asio::as_tuple(use_awaitable));
    check_ec(ec, __func__);

    asio::error_code ec_formal;
    auto rc = socket_.shutdown(tcp::socket::shutdown_both, ec_formal);
    check_ec(rc, __func__);
};

HTTPSession::OutgoingSession::~OutgoingSession()
{
    gl_logger->trace("OutgoingSession destructed, id: {}...", context_.session_id);
}

awaitable<void> HTTPSession::OutgoingSession::start()
{
    if (!init_ssl())
    {
        gl_logger->error("SSL initialization failure!");
        co_return;
    }

    gl_logger->info("OutgoingSession started, id: {}", context_.session_id);

    auto self = shared_from_this();

    auto [ec, results] =
        co_await resolver_.async_resolve(HOST, PORT, asio::as_tuple(use_awaitable));
    if (check_ec(ec, __func__))
    {
        co_await connect(results);
    }
}

awaitable<void> HTTPSession::OutgoingSession::connect(const tcp::resolver::results_type& endpoints)
{
    auto [ec, _] =
        co_await asio::async_connect(stream_.next_layer(), endpoints, asio::as_tuple(use_awaitable));

    if (check_ec(ec, __func__))
    {
        co_await on_connect();
    }
};

awaitable<void> HTTPSession::OutgoingSession::on_connect()
{
    gl_logger->info("OutgoingSession connected, id: {}", context_.session_id);

    co_await stream_.async_handshake(asio::ssl::stream_base::client, use_awaitable);

    co_await send_request();
}

awaitable<void> HTTPSession::OutgoingSession::send_request()
{
    generate_request();

    auto [ec, _] =
        co_await asio::async_write(stream_, asio::buffer(http_request_), asio::as_tuple(use_awaitable));

    if (check_ec(ec, __func__))
    {
        co_await read_response();
    }
}

awaitable<void> HTTPSession::OutgoingSession::read_response()
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

void HTTPSession::OutgoingSession::generate_request()
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

bool HTTPSession::OutgoingSession::init_ssl()
{
    // Verify server certificate (important)
    stream_.set_verify_mode(asio::ssl::verify_peer);
    stream_.set_verify_callback(asio::ssl::host_name_verification(HOST));

    // SNI (many hosts require it)
    bool result = SSL_set_tlsext_host_name(stream_.native_handle(), HOST.c_str());
    if (!result)
    {
        gl_logger->error("Failed to set SNI host name {}", HOST);
    }
    return result;
}
