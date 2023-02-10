#ifndef ARB_ARBITRAGE_H
#define ARB_ARBITRAGE_H

#include <cinttypes>
#include "../include/rapidjson/document.h"
#include <chrono>
#include "web.h"
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <utility>

struct DepthData {
public:
    double buy_price;
    double buy_amount;
    double sell_price;
    double sell_amount;

    friend std::ostream& operator<<(std::ostream& os, const DepthData& r) {
        os << "BinanceResult { "
           << "\n\tbuy_price: " << r.buy_price
           << "\n\tbuy_amount: " << r.buy_amount
           << "\n\tsell_price: " << r.sell_price
           << "\n\tsell_amount: " << r.sell_amount
           << "\n}";
        return os;
    }
};

DepthData parse_depth_data(rapidjson::Document& doc, double amount) {
    double buy_price = 0;
    double sell_price = 0;

    double buy_amount = 0;
    const rapidjson::Value& asks_json = doc["a"];
    for (rapidjson::SizeType j = 0; j < asks_json.Size(); ++j) {
        double p = std::stod(asks_json[j][0].GetString());
        double a = std::stod(asks_json[j][1].GetString());

        double add = std::min(amount - buy_amount, 1/a);
        buy_amount += add;
        buy_price += 1/p * add;
        if (amount - buy_amount <= 0) break;
    }

    double sell_amount = 0;
    const rapidjson::Value& bids_json = doc["b"];
    for (rapidjson::SizeType j = 0; j < bids_json.Size(); ++j) {
        double p = std::stod(bids_json[j][0].GetString());
        double a = std::stod(bids_json[j][1].GetString());

        double add = std::min(amount - sell_amount, 1/a);
        sell_amount += add;
        sell_price += 1/p * add;
        if (amount - sell_amount <= 0) break;
    }

    return DepthData{buy_price, buy_amount, sell_price, sell_amount};
}

class Arbitrage {
private:
    uint64_t iterations{};
    int buy_delay;
    int sell_delay;
    double activation_threshold;
    double previous_sell_price{};
    double current_sell_price{};
    bool useCurrencyForAmount;
    double crypto_current_amount{};
    double crypto_buy_amount;
    std::string symbol;
    std::chrono::time_point<std::chrono::steady_clock> crypto_buy_timestamp;
    RestApi rest_api;
    WebSockets web_sockets;
    std::unordered_map<std::string, double> symbol_step_sizes;
    std::ofstream& log_file;
private:
    void update_exchange_info() {
        auto result = rest_api.get("/api/v3/exchangeInfo", "", false);
        rapidjson::Document doc;
        doc.Parse(result.second.body().c_str());

        symbol_step_sizes.clear();

        auto symbols = doc["symbols"].GetArray();
        for (rapidjson::SizeType i = 0; i < symbols.Size(); ++i) {
            std::string s = symbols[i]["symbol"].GetString();
            auto filters = symbols[i]["filters"].GetArray();
            double step = 0;
            for (rapidjson::SizeType j = 0; j < filters.Size(); ++j) {
                if (strcmp(filters[j]["filterType"].GetString(), "LOT_SIZE") != 0) continue;
                step = std::stod(filters[j]["stepSize"].GetString());
                break;
            }

            symbol_step_sizes.emplace(s, step);
        }
    }

    void subscribe_to_data() {
        std::string symbol_lowercase = symbol;
        std::transform(symbol_lowercase.begin(), symbol_lowercase.end(), symbol_lowercase.begin(), [](unsigned char c){ return std::tolower(c); } );
        std::string ws_connection_msg =  "{"
                                         "\"method\": \"SUBSCRIBE\","
                                         "\"params\": [\"" + symbol_lowercase + "@depth@100ms\"],"
                                         "\"id\": 0"
                                         "}";

        web_sockets.write(ws_connection_msg);
        web_sockets.read();
        rapidjson::Document doc;
        doc.Parse(web_sockets.read_str_from_buffer().c_str());
        if (!doc.HasMember("result"))
            std::cout << BinanceResult{doc["code"].GetInt(), doc["msg"].GetString()} << std::endl;
    }

    double fix_price(double v, bool useCurrency) {
        double step = symbol_step_sizes[symbol];
        step = 1.0 / step;
//        if (useCurrency) step = 1.0 / step;
        return floor(v * step) / step;
    }

    std::string new_market_order(bool isBuy, double quantity, bool useQuoteOrderQty) {
        std::string target = "/api/v3/order";
        std::string query = "type=MARKET&symbol=" + symbol + "&side=" + (isBuy ? "BUY" : "SELL") + (useQuoteOrderQty ? "&quoteOrderQty=" : "&quantity=") + std::to_string(quantity);
        auto result = rest_api.post(target, query, true);
        return result.second.body();
    }

    static std::tuple<double, double, double> parse_market_order_result(rapidjson::Document& doc) {
        //todo check error
        double orig = std::stod(doc["origQty"].GetString());
        double executed = std::stod(doc["executedQty"].GetString());
        double cum = std::stod(doc["cummulativeQuoteQty"].GetString());
        return {orig, executed, cum};
    }

    void sell_crypto() {
        double rounded_amount = fix_price(crypto_current_amount, !useCurrencyForAmount);
        std::cout << "selling " << rounded_amount << std::endl;
        std::string result_str = new_market_order(false, rounded_amount, !useCurrencyForAmount);
        std::cout << result_str << std::endl;
        log_file << result_str << ",\n\t";

        rapidjson::Document doc;
        doc.Parse(result_str.c_str());
        auto result = parse_market_order_result(doc);
        std::cout << "sold " << std::get<1>(result) << " (" << std::get<2>(result) << ") of " << rounded_amount << std::endl;
        crypto_current_amount = 0;
    }

    void buy_crypto() {
        double rounded_amount = fix_price(crypto_buy_amount, useCurrencyForAmount);
        std::cout << "buying " << rounded_amount << std::endl;
        std::string result_str = new_market_order(true, rounded_amount, useCurrencyForAmount);
        std::cout << result_str << std::endl;
        log_file << result_str << ",\n\t";

        rapidjson::Document doc;
        doc.Parse(result_str.c_str());
        auto result = parse_market_order_result(doc);
        std::cout << "bought " << std::get<1>(result) << " (" << std::get<2>(result) << ") of " << rounded_amount << std::endl;
        crypto_current_amount = std::get<1>(result);
        crypto_buy_timestamp = std::chrono::steady_clock::now();
    }

public:
    Arbitrage(std::string symbol, const std::string& rest_host, const std::string& ws_host, std::string key, std::string secret, double crypto_buy_amount, net::io_context& io, net::ssl::context& ssl, bool useCurrencyForAmount, int buy_delay, int sell_delay, double activation_threshold, std::ofstream& log_file) :
            buy_delay(buy_delay),
            sell_delay(sell_delay),
            activation_threshold(activation_threshold),
            useCurrencyForAmount(useCurrencyForAmount),
            crypto_buy_amount(crypto_buy_amount),
            symbol(std::move(symbol)),
            rest_api(BinanceRestApi(rest_host, std::move(key), std::move(secret), io, ssl)),
            web_sockets(BinanceWebSockets(ws_host, "/ws", io, ssl)),
            log_file(log_file)
    {
        update_exchange_info();
        subscribe_to_data();
    }

    void update() {
        web_sockets.read();
        rapidjson::Document doc;
        doc.Parse(web_sockets.read_str_from_buffer().c_str());
        auto depth = parse_depth_data(doc, crypto_buy_amount);

        if (depth.sell_amount >= crypto_buy_amount) {
            set_new_sell_price(depth.sell_price);

            auto now = std::chrono::steady_clock::now();
            if (crypto_current_amount <= 0) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - crypto_buy_timestamp).count() >= buy_delay)
                    buy_crypto();
                return;
            }

            double price_change = get_sell_price_change_percent();
            if (price_change < -activation_threshold || price_change >= activation_threshold) {
                sell_crypto();
                return;
            }
            if (std::chrono::duration_cast<std::chrono::seconds>(now - crypto_buy_timestamp).count() >= sell_delay)
                sell_crypto();
        }
    }

    double get_sell_price_change_percent() const {
        if (iterations < 2) return 0;
        return (current_sell_price / previous_sell_price) - 1.0;
    }

    void set_new_sell_price(double v) {
        iterations++;
        previous_sell_price = current_sell_price;
        current_sell_price = v;
    }

    void close() {
        web_sockets.close();
        rest_api.close();
    }
};


#endif //ARB_ARBITRAGE_H
