#include "http-session.h"

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <spdlog/fmt/fmt.h>

#include "logs/logger.h"

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

    if (!init_tls_context())
    {
        gl_logger->info("HTTPSession failed, id: {} ...", id_);
    }
    else
    {
        obtain_header();
    }
}

template <typename T>
void HTTPSession<T>::stop()
{
    gl_logger->info("HTTPSession session, stop pending, id: {} ...", id_);
    stopped_ = true;
}

template <typename T>
bool HTTPSession<T>::init_tls_context()
{
    bool result(true);
    std::string failure_hints;

    try
    {
#ifdef _WIN32
        const std::string cert_path = "certs\\cacert.pem";
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

    asio::async_read_until(
        http_stream_, buffer_, http_request_headers_delimiter,
        asio::bind_executor(strand_,
                            [this, self](const asio::error_code& ec, std::size_t bytes_transferred)
                            {
                                if (check_ec(ec, __func__))
                                {
                                    std::istream stream(&buffer_);
                                    raw_request_.resize(bytes_transferred);
                                    stream.read(&raw_request_[0], bytes_transferred);

                                    HttpRequest request = parse_request(raw_request_);
                                    on_request(request);
                                }
                            }));
}

template <typename T>
void HTTPSession<T>::on_request(HttpRequest request)
{
    if (stopped_)
        return;

    auto self = this->shared_from_this();
    request_ = std::move(request);

    auto outgoing_session = std::make_shared<HTTPSession<T>::OutgoingSession>(self);
    outgoing_session->start();
}

template <typename T>
void HTTPSession<T>::on_outgoing_session_completed(const asio::error_code& ec, std::string response)
{
    gl_logger->info("OutgoingSession completed, id: {}", id_);
    gl_logger->trace("Response {}", response);

    response_ = std::move(response);

    auto self = this->shared_from_this();

    auto buff = asio::buffer(response_);
    asio::async_write(
        http_stream_, buff,
        asio::bind_executor(strand_,
                            [this, self](const asio::error_code& ec, std::size_t)
                            {
                                asio::error_code ec_formal;

                                if constexpr (std::is_same_v<T, tcp::socket>)
                                    auto rc = http_stream_.shutdown(tcp::socket::shutdown_both, ec_formal);
                            }));
};

template <typename T>
HTTPSession<T>::OutgoingSession::~OutgoingSession()
{
    gl_logger->trace("OutgoingSession destructed id: {}...", context_.session_id);
}

template <typename T>
void HTTPSession<T>::OutgoingSession::start()
{
    if (!init_ssl())
    {
        gl_logger->error("SSL initialization failure!");
        return;
    }

    gl_logger->info("OutgoingSession started, id: {}", context_.session_id);

    auto self = this->shared_from_this();

    resolver_.async_resolve(HOST, PORT,
                            asio::bind_executor(context_.strand,
                                                [this, self](const asio::error_code& ec,
                                                             tcp::resolver::results_type results)
                                                {
                                                    if (check_ec(ec, __func__))
                                                    {
                                                        connect(results);
                                                    }
                                                }));
}

template <typename T>
void HTTPSession<T>::OutgoingSession::connect(const tcp::resolver::results_type& endpoints)
{
    auto self = this->shared_from_this();

    asio::async_connect(
        stream_.next_layer(), endpoints,
        asio::bind_executor(context_.strand,
                            [this, self](const asio::error_code& ec, const tcp::endpoint&)
                            {
                                if (check_ec(ec, __func__))
                                {
                                    on_connect();
                                }
                            }));
};

template <typename T>
void HTTPSession<T>::OutgoingSession::on_connect()
{
    gl_logger->info("OutgoingSession connected, id: {}", context_.session_id);

    auto self = this->shared_from_this();

    stream_.async_handshake(asio::ssl::stream_base::client,
                            asio::bind_executor(context_.strand,
                                                [this, self](const asio::error_code& ec)
                                                {
                                                    if (check_ec(ec, __func__))
                                                    {
                                                        send_request();
                                                    }
                                                }));
}

template <typename T>
void HTTPSession<T>::OutgoingSession::send_request()
{
    auto self = this->shared_from_this();

    generate_request();

    asio::async_write(stream_, asio::buffer(http_request_),
                      asio::bind_executor(context_.strand,
                                          [this, self](const asio::error_code& ec, std::size_t)
                                          {
                                              if (check_ec(ec, __func__))
                                              {
                                                  read_response();
                                              }
                                          }));
}

template <typename T>
void HTTPSession<T>::OutgoingSession::read_response()
{
    auto self = this->shared_from_this();

    stream_.async_read_some(
        asio::buffer(buffer_),
        asio::bind_executor(
            context_.strand,
            [this, self](const asio::error_code& ec, std::size_t n)
            {
                if (is_eof(ec))
                {
                    std::string resp(response_.str());
                    outer_session_->on_outgoing_session_completed(ec, std::move(resp));
                }
                else if (check_ec(ec, __func__))
                {
                    response_.write(buffer_.data(), static_cast<std::streamsize>(n));
                    read_response(); // continue reading
                }
            }));
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

// explicit specialization:
template class HTTPSession<tcp::socket>;
template class HTTPSession<asio::ssl::stream<tcp::socket>>;