#include <gtest/gtest.h>

#include <string>

#include "utils/http-helper.h"

namespace
{

    TEST(GenerateHttpResponseTest, GeneratesTextResponseWithDefaultOptions)
    {
        const auto response = generate_http_response(
            HTTPResponseCodes::BadGateway, "Bad Gateway", "Upstream unavailable");

        EXPECT_EQ(response.status_code, 502);
        EXPECT_EQ(response.reason, "Bad Gateway");
        EXPECT_EQ(response.body, "Upstream unavailable");
        EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");
        EXPECT_EQ(response.headers.at("Content-Length"), "20");
        EXPECT_EQ(response.headers.at("Connection"), "close");
    }

    TEST(GenerateHttpResponseTest, GeneratesJsonKeepAliveResponse)
    {
        const std::string body = R"({"status":"UP"})";

        const auto response = generate_http_response(
            HTTPResponseCodes::OK,
            "OK",
            body,
            HTTPResponseContentType::ApplicationJson,
            HTTPResponseConnection::KeepAlive);

        EXPECT_EQ(response.status_code, 200);
        EXPECT_EQ(response.reason, "OK");
        EXPECT_EQ(response.body, body);
        EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
        EXPECT_EQ(response.headers.at("Content-Length"), std::to_string(body.size()));
        EXPECT_EQ(response.headers.count("Connection"), 0u);
    }

    TEST(GenerateHttpResponseTest, GeneratesEmptyResponseWithoutContentType)
    {
        const auto response = generate_http_response(
            HTTPResponseCodes::RequestTimeout, "Request Timeout", "");

        EXPECT_EQ(response.status_code, 408);
        EXPECT_EQ(response.reason, "Request Timeout");
        EXPECT_TRUE(response.body.empty());
        EXPECT_EQ(response.headers.at("Content-Length"), "0");
        EXPECT_EQ(response.headers.at("Connection"), "close");
        EXPECT_EQ(response.headers.count("Content-Type"), 0u);
    }

    TEST(HttpResponseToStringTest, AddsContentLengthWhenHeaderIsMissing)
    {
        HttpResponse response;
        response.status_code = 201;
        response.reason = "Created";
        response.headers["Content-Type"] = "text/plain";
        response.body = "created";

        EXPECT_EQ(response.to_string(),
                  "HTTP/1.1 201 Created\r\n"
                  "Content-Type: text/plain\r\n"
                  "Content-Length: 7\r\n"
                  "\r\n"
                  "created");
    }

    TEST(HttpResponseToStringTest, PreservesExplicitContentLengthWithoutDuplicatingIt)
    {
        HttpResponse response;
        response.status_code = 204;
        response.reason = "No Content";
        response.headers["Connection"] = "close";
        response.headers["Content-Length"] = "0";

        const auto serialized = response.to_string();

        EXPECT_EQ(serialized,
                  "HTTP/1.1 204 No Content\r\n"
                  "Connection: close\r\n"
                  "Content-Length: 0\r\n"
                  "\r\n");
    }

    TEST(HttpResponseToStringTest, SerializesGeneratedResponse)
    {
        const auto response = generate_http_response(
            HTTPResponseCodes::BadGateway, "Bad Gateway", "");

        EXPECT_EQ(response.to_string(),
                  "HTTP/1.1 502 Bad Gateway\r\n"
                  "Connection: close\r\n"
                  "Content-Length: 0\r\n"
                  "\r\n");
    }

} // namespace
