#include <iostream>

#include <string>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <atomic>

#include "include/rapidjson/document.h"

#include "src/web.h"
#include "src/arbitrage.h"

int main(int argc, char* argv[]) {
    std::cout << "found " << argc << " args" << std::endl;
    if (argc != 9) {
        std::cout << "===\n"
                     "arb <rest api url> <key> <secret> <symbol> <start amount> <buy delay> <max sell delay> <activation threshold>\n"
                     "where:"
                     "\t<rest api url>: url for binance server for orders ('testnet.binance.vision' for testnet and 'https://api.binance.com' for real)\n"
                     "\t<key>: key for api with orders permission (for example: 'QBSsOhuQhdforTQXPASGOWlfNUPP0WL2d14YN525JRRqjzIfVTL1D7Jdqx0Ki2cH')\n"
                     "\t<secret>: api secret for key (for example: 'Ghhd4ipSVR2bwg123cHahXjbkExuV5sZuRxTPuno2c145kzkdpmiDJodF3u0rpIk')\n"
                     "\t<symbol>: identifier for a single crypto/currency (for example: 'BTCUSDT'). Make sure it's valid on current exchange\n"
                     "\t<start amount>: amount to buy, in currency (for example: '15'; 15 of BTCUSDT will buy BTC using 15 USDT)\n"
                     "\t<buy delay>: delay in seconds between selling and buying crypto (for example: '30')\n"
                     "\t<max sell delay>: maximum delay in seconds after which crypto will be sold (for example: '60')\n"
                     "\t<activation threshold>: how sensitive this script to price change, in 'factor' (for example: '0.0001'; 0.0001 is 0.01%)\n"
                     "Note: all parameters are without quotation marks (<\'> or <\">)\n"
                     "===\n"
                     "Example:\n"
                     "arb testnet.binance.vision QBSsOhuQhdforTQXPASGOWlfNUPP0WL2d14YN525JRRqjzIfVTL1D7Jdqx0Ki2cH Ghhd4ipSVR2bwg123cHahXjbkExuV5sZuRxTPuno2c145kzkdpmiDJodF3u0rpIk BTCUSDT 5 30 60 0.0001"
                     "\n" << std::endl;
        throw std::runtime_error("Invalid argument count!");
    }
    char** args = argv + 1;
    std::string rest_api_host = *args++;
    std::string api_key = *args++;
    std::string api_secret = *args++;
    std::string ws_host = "stream.binance.com";
    std::string symbol = *args++;
    double start_amount = std::stod(*args++);
    int buy_delay = std::stoi(*args++);
    int max_sell_delay = std::stoi(*args++);
    double activation_threshold = std::stod(*args++);

    std::ofstream log_file("log.json");
    log_file.clear();
    log_file << "[   \n\t";

    try {
        auto io = net::io_context{};
        auto ssl = net::ssl::context{net::ssl::context::tlsv12_client};
        Arbitrage arb(symbol, rest_api_host, ws_host, api_key, api_secret, start_amount, io, ssl, true, buy_delay, max_sell_delay, activation_threshold, log_file);

        std::atomic<bool> quit = false;
        std::thread loop_ctrl_thread([&]() {
            getchar();
            quit = true;
        });

        std::cout << "connected" << std::endl;
        while (!quit) arb.update();
        loop_ctrl_thread.join();
        arb.close();
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "exit" << std::endl;
    log_file << "\n]";
    log_file.close();
    return 0;
}