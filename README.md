# SimpleArbitrage
Simple Binance arbitrage written in C++. <br/><br/>
Based on:
- [Boost::Beast](https://github.com/boostorg/beast)
- [OpenSSL](https://github.com/openssl/openssl)
- [RapidJson](https://github.com/Tencent/rapidjson)

# Installation
- Make sure you have Boost and OpenSSL
  - Arch: ```pacman -S boost openssl```
  - Ubuntu: ```sudo apt-get libboost-all-dev libssl-dev```
- Open directory for project, for example `~/temp`
- Clone and build
```
git clone https://github.com/firedef/SimpleArbitrage
cd SimpleArbitrage
cmake . -DCMAKE_BUILD_TYPE=Release && make
```

# Usage
To run this you need to call it from console with arguments: <br/>
`
./arb <rest api url> <key> <secret> <symbol> <start amount> <buy delay> <max sell delay> <activation threshold>
`
where:
- `<rest api url>`: url for binance server for orders ('testnet.binance.vision' for testnet and 'api.binance.com' for real)
- `<key>`: key for api with orders permission (for example: *'QBSsOhuQhdforTQXPASGOWlfNUPP0WL2d14YN525JRRqjzIfVTL1D7Jdqx0Ki2cH'*)
- `<secret>`: api secret for key (for example: *'Ghhd4ipSVR2bwg123cHahXjbkExuV5sZuRxTPuno2c145kzkdpmiDJodF3u0rpIk'*)
- `<symbol>`: identifier for a single crypto/currency (for example: 'BTCUSDT'). Make sure it's valid on current exchange
- `<start amount>`: amount to buy, in currency (for example: '15'; 15 of BTCUSDT will buy BTC using 15 USDT). Must be more than MIN_NOTIONAL of the asset (usually >10$)
- `<buy delay>`: delay in seconds between selling and buying crypto (for example: '30')
- `<max sell delay>`: maximum delay in seconds after which crypto will be sold (for example: '60')
- `<activation threshold>`: how sensitive this script to price change, in 'factor' (for example: '0.0001'; 0.0001 is 0.01%)

So it should looks like this: <br/>
`
./arb testnet.binance.vision QBSsOhuQhdforTQXPASGOWlfNUPP0WL2d14YN525JRRqjzIfVTL1D7Jdqx0Ki2cH Ghhd4ipSVR2bwg123cHahXjbkExuV5sZuRxTPuno2c145kzkdpmiDJodF3u0rpIk BTCUSDT 15 30 60 0.0001
`

Pass all parameters without quotation marks (<'> or <">). Don't use api key and secret from example - they are invalid, but should looks like this. <br/>

Logs are available in `log.json` next to program. SimpleArbitrage will clear them on next run. <br/>

# How to improve
- Fix 'connection reset by peer' by reconnecting websockets and resending rest api requests. Right now it may crash randomly
- Better error handle and more checks. Right now app may crash parsing json with exchange error
- Make writes and reads asynchronous
- Add support for multiple assets. You can subscribe to multiple assets in websockets.
- Better result log
