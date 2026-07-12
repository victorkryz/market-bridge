# $\color{MidnightBlue}\textit{\textbf{market-bridge}}$


![C++](https://img.shields.io/badge/C++-20-purple?logo=C++)
![asio](https://img.shields.io/badge/asio-1.36.0-lightblue?logo=asio)
![cmake](https://img.shields.io/badge/cmake-3.30-lightgray)
![gtest](https://img.shields.io/badge/GTest-1.14.0-blueviolet)
![doctest](https://img.shields.io/badge/doctest-2.4.12-brightgreen)
![Ubuntu](https://img.shields.io/badge/Ubuntu-18.04+-red?logo=Ubuntu)
![Windows](https://img.shields.io/badge/Windows-11-blue?logo=Windows)


**Market-bridge**  is a lightweight, high-performance C++ proxy server for the Binance Open API, implemented using the standalone Asio library.
It acts as a transparent gateway that forwards client requests to Binance and send responses back without modification.

### The proxy execution workflow:

-  The proxy listens on localhost:8080 (by default)
-  Accepts and parsers incoming HTTP requests
-  Parsers API request
-  Establishes outgoing connection
-  Forwards client's HTTP payload to api.binance.com 
   (the original request's part is preserved)
-  Returns Binance response to the client


The proxy mirrors Binance Open API endpoints through a local proxy interface.  
When the proxy is running on localhost:8080, any request directly sent to: https://api.binance.com
can instead be redirected to: http://localhost:8080.  
The request path remains unchanged.


For example:

- direct calls to Binance:
``` 
  curl https://api.binance.com/api/v3/ping
  curl https://api.binance.com/api/v3/time
  curl https://api.binance.com/api/v3/ticker/price
```
- respective proxy calls:

``` 
  curl http://localhost:8080/api/v3/ping
  curl http://localhost:8080/api/v3/time
  curl http://localhost:8080/api/v3/ticker/price
```


### Command line arguments:

```
-h, --help             print usage
-p, --http-port arg    specify http port (default: 8080)
-s, --https-port arg   specify port (default: 8443)
-o, --log-output arg   specify logging output (file, console) (default: 
                        console)
-r, --run-mode arg     specify running mode (persist, single-request) 
                        (default: persist)
-l, --log-level arg    specify log level (error, warning, trace, debug, 
                        critical, off) (default: info)
-c, --cert-path arg    specify https server certificate path (default: 
                        cert/server.crt)
-k, --private-key arg  specify https server private key path (default: 
                        cert/server.key)
-i, --ignore-cert-verification  ignore SSL certificate verification for 
                                outgoing requests
-H, --upstream-host arg  specify upstream host for proxying outgoing 
                         requests (default: api.binance.com)
-P, --upstream-port arg  specify upstream port for proxying outgoing 
                         requests (default: 443)
    --config arg         load configuration from a JSON file
```

### JSON configuration:

The JSON configuration file can contain the following values (shown with their defaults):

```json
{
  "server": {
    "http_port": 8080,
    "https_port": 8443,
    "run_mode": "persist",
    "allow_https_over_http_port": false
  },
  "tls": {
    "certificate": "cert/server.crt",
    "private_key": "cert/server.key"
  },
  "upstream": {
    "host": "api.binance.com",
    "port": 443,
    "ignore_certificate_verification": false
  },
  "logging": {
    "output": "console",
    "level": "info"
  }
}
```

Command line parameters override values specified in the JSON configuration file.

### Samples of usage:

```
market-bridge
market-bridge -p 8080
market-bridge -l info
market-bridge -h
market-bridge -o file
market-bridge --config market-bridge.json
market-bridge --config market-bridge.json --http-port 9000
```

### Third-Party Libraries:

- Standalone Asio (https://github.com/chriskohlhoff/asio)
- "Lightweight C++ command line option parser (https://github.com/jarro2783/cxxopts)
- "spdlog" for logging (see: https://github.com/gabime/spdlog)
- OpenSSL development package is required   
  (e.g. on Debian-based systems: sudo apt install libssl-dev)


### How to build:
-------------------------------------------------------------------------

Project building is managed and defined by CMake.

To build the project under Linux OS use build.sh script with build type specification:

```
    ./build.sh release (debug)
```
 or use cmake directly:

```
    cmake -S . -B build
    cmake --build  build
```

#### Branches:

 - **main** -  C++17 implementation using ASIO asynchronous APIs with lambda handlers
 - **dev/cpp20** - C++20 implementation using ASIO coroutines (in-progress)



### Validation tests
-------------------------------------------------------------------------

The proxy server has been validated with several tests.

### 1. Response integrity

Responses returned via the proxy are identical to direct Binance responses.

```
  diff <(curl -s http://localhost:8080/api/v3/price) <(curl -s https://api.binance.com/api/v3/price)
```  

### 2. Latency overhead

The proxy adds only minimal latency (~few milliseconds)

```
  curl -w "%{time_total}\n" -o /dev/null -s https://api.binance.com/api/v3/time
  curl -w "%{time_total}\n" -o /dev/null -s http://localhost:8080/api/v3/time
```  

### 3. Concurrent client handling

The proxy successfully handled 1000 requests with 50 concurrent clients

```
  hey -n 1000 -c 50 http://localhost:8080/api/v3/time
```  

### 4. Integration test

The proxy has been tested using the Python project 
[*order-book-viewer-py*](https://github.com/victorkryz/order-book-viewer-py), which successfully  
retrieved order-book data via the proxy: border-book-viewer-py  →  market-bridge proxy  →  api.binance.com

```
  python order-book-view.py --host http://localhost:8080
```  

### 5. Memory leaks check

```
  valgrind -s --leak-check=yes build/market-bridge
```
