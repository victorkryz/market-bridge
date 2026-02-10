#pragma once

#include <asio.hpp>
#include <map>
#include <string>

enum class HTTPResponseCodes : long
{
    OK = 200,
    Created = 201,
    NoContent = 204,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404
};

inline constexpr auto http_request_headers_delimiter = "\r\n\r\n";

inline bool is_http_status_ok(long status)
{
    return (static_cast<long>(HTTPResponseCodes::OK) == status);
}

struct UrlEntry
{
    std::string original;
    std::uint64_t created_at; // seconds since start (simple)
    std::uint64_t hits;
};

struct HttpRequest
{
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse
{
    int status_code = static_cast<long>(HTTPResponseCodes::OK);
    std::string reason = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    std::string to_string() const
    {
        auto constexpr rn = "\r\n";

        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << reason << rn;
        for (auto const& kv : headers)
        {
            oss << kv.first << ": " << kv.second << rn;
        }
        oss << "Content-Length: " << body.size() << rn;
        oss << rn;
        oss << body;
        return oss.str();
    }
};

inline HttpRequest parse_request(const std::string& raw)
{
    HttpRequest req;
    std::istringstream iss(raw);
    std::string line;

    // Request line
    if (!std::getline(iss, line))
    {
        return req;
    }
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    {
        std::istringstream rl(line);
        rl >> req.method >> req.target >> req.version;
    }

    // Headers
    while (std::getline(iss, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            break; // end of headers
        auto pos = line.find(':');
        if (pos != std::string::npos)
        {
            std::string name = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // trim leading spaces
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            {
                value.erase(value.begin());
            }
            req.headers[name] = value;
        }
    }

    // Body (rest of stream)
    std::string body;
    std::getline(iss, body, '\0');
    req.body = body;

    return req;
}
