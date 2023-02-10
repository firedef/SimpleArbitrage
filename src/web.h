#ifndef ARB_WEB_H
#define ARB_WEB_H

#include <string>
#include <utility>
#include <ostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <iomanip>
#include <chrono>
#include "../include/rapidjson/document.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

struct BinanceResult {
public:
    int code;
    std::string msg;

    friend std::ostream& operator<<(std::ostream& os, const BinanceResult& r) {
        os << "BinanceResult { "
           << "\n\tcode: " << r.code
           << "\n\tmsg: " << r.msg
           << "\n}";
        return os;
    }
};

class WebSockets {
private:
    //websocket::stream<tcp::socket> ws;
    websocket::stream<beast::ssl_stream<tcp::socket>> ws;
public:
    beast::flat_buffer read_buffer;

public:
    WebSockets(const std::string& host, const std::string& target, net::io_context& io, net::ssl::context& ssl) : ws{io, ssl}, read_buffer() {
        // create endpoint
        tcp::resolver resolver(io);
        auto const results = resolver.resolve(host, "https");
        net::connect(ws.next_layer().next_layer(), results);

        // say hello to the server
        ws.next_layer().handshake(net::ssl::stream_base::client);

        // set additional data to requests
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) { req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING); }
        ));

        ws.handshake(host, target);
    }

    template<class ConstBufferSequence>
    [[maybe_unused]] void write(ConstBufferSequence const& buffers) { ws.write(buffers); }

    [[maybe_unused]] void write(std::string& msg) { ws.write(net::buffer(msg)); }

    [[maybe_unused]] size_t read() { read_buffer.clear(); return ws.read(read_buffer); }

    [[maybe_unused]] std::string read_str_from_buffer() { return beast::buffers_to_string(read_buffer.data()); }

    [[maybe_unused]] void pong() { ws.pong(websocket::ping_data()); }

    [[maybe_unused]] void close() {
//        ws.next_layer().next_layer().close();

        boost::system::error_code ec;
        ws.close(websocket::close_reason(), ec);
        if (ec == boost::asio::ssl::error::stream_truncated) { return; }
        throw boost::system::system_error{ec};
    }
};

class BinanceWebSockets : public WebSockets {
public:
    BinanceWebSockets(const std::string& base_url, const std::string& target, net::io_context& io, net::ssl::context& ctx) : WebSockets(base_url, target, io, ctx) { }

    BinanceResult subscribe(std::string& msg) {
        write(msg);
        if (read() == 0) return BinanceResult{0,""};
        rapidjson::Document doc;
        doc.Parse(read_str_from_buffer().c_str());
        if (doc.HasMember("result")) return BinanceResult{0,""};
        return BinanceResult{doc["code"].GetInt(), doc["msg"].GetString()};
    }
};

class RestApi {
private:
    std::string api_key;
    std::string api_secret;
    std::string host;
    boost::asio::ssl::stream<tcp::socket> stream;
    bool sign;
public:
    beast::flat_buffer read_buffer;

    RestApi(const std::string& host, std::string&& key, std::string&& secret, net::io_context& io, net::ssl::context& ssl) : api_key(key), api_secret(secret), host(host), stream{io, ssl}, sign(key.length() > 0), read_buffer() {
        // set SNI
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
            throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()), "Failed to set SNI Hostname");

        tcp::resolver resolver(io);
        auto const results = resolver.resolve(host, "https");
        net::connect(stream.next_layer(), results.begin(), results.end());
        stream.handshake(net::ssl::stream_base::client);
    }

private:
    template<typename TIter>
    std::string make_hex_string(TIter first, TIter last) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        while (first != last)
            ss << std::setw(2) << static_cast<int>(*first++);
        return ss.str();
    }
    std::string calc_hmacsha256(std::string_view key, std::string_view msg) {
        std::array<uint8_t, EVP_MAX_MD_SIZE> hash{};
        uint32_t hash_len;
        HMAC(
                EVP_sha256(),
                key.data(),
                static_cast<int>(key.size()),
                reinterpret_cast<const uint8_t*>(msg.data()),
                static_cast<int>(msg.size()),
                hash.data(),
                &hash_len
        );
        // signature for Binance must be a sequence of lower-case hex chars (0-f)
        return make_hex_string(hash.begin(), hash.begin() + hash_len);
    }
    std::string sign_in(std::string& query) {
        return calc_hmacsha256(api_secret, query);
    }
public:
    [[maybe_unused]] void write(http::request<http::string_body> request, bool use_sign = false) {
        request.set(http::field::host, host);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        if (use_sign) request.set("X-MBX-APIKEY", api_key);
        http::write(stream, request);
    }
    [[maybe_unused]] std::pair<size_t, http::response<http::string_body>> read() {
        http::response<http::string_body> response;
        return std::make_pair(http::read(stream, read_buffer, response), response);
    }
    [[maybe_unused]] std::pair<size_t, http::response<http::string_body>> get(const std::string& target, bool use_sign = false) {
        write(http::request<http::string_body>{http::verb::get, target, 11}, use_sign);
        return read();
    }
    [[maybe_unused]] std::pair<size_t, http::response<http::string_body>> post(const std::string& target, bool use_sign = false) {
        write(http::request<http::string_body>{http::verb::post, target, 11}, use_sign);
        return read();
    }

    [[maybe_unused]] std::string make_target(std::string target, std::string query, bool use_sign = false) {
        if (use_sign) {
            if (!sign) throw std::runtime_error("Api key or api secret is missing");
            uint64_t time_ms = std::chrono::system_clock::now().time_since_epoch().count() / 1000 / 1000; // chrono time is in nanoseconds so convert it to milliseconds (unix time)
            query += "&timestamp=" + std::to_string(time_ms); // time is only required on sign in
            query += "&signature=" + sign_in(query);
        }
        if (query.length() > 0) target += '?' + query;
        return target;
    }
    [[maybe_unused]] std::pair<size_t, http::response<http::string_body>> get(std::string target, std::string query, bool use_sign = false) {
        return get(make_target(std::move(target), std::move(query), use_sign), use_sign);
    }
    [[maybe_unused]] std::pair<size_t, http::response<http::string_body>> post(std::string target, std::string query, bool use_sign = false) {
        return post(make_target(std::move(target), std::move(query), use_sign), use_sign);
    }

    [[maybe_unused]] std::string read_str_from_buffer() { return beast::buffers_to_string(read_buffer.data()); }

    [[maybe_unused]] void close() {
        boost::system::error_code ec;
        stream.shutdown(ec);
        if (ec == boost::asio::ssl::error::stream_truncated) { return; }
        if (ec) throw boost::system::system_error{ec};
    }
};

class BinanceRestApi : public RestApi {
public:
    [[maybe_unused]] BinanceRestApi(const std::string& host, std::string key, std::string secret, net::io_context& io, net::ssl::context& ssl) : RestApi(host, std::move(key), std::move(secret), io, ssl) { }
};


#endif //ARB_WEB_H
