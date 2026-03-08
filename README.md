# $\color{MidnightBlue}\textit{\textbf{market-bridge}}$


![C++](https://img.shields.io/badge/C++-17/20-purple?logo=C++)
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
-h, --help            print usage
-p, --port arg        specify port (default: 8080)
-o, --log-output arg  specify logging output (file, console) (default: console)
                      (in case 'file' is set the log files located in ~/.local/share/market-bridge)
-l, --log_level arg   specify log level (error, warning, trace, debug, 
                                         critical, off) (default: info)

```

### Samples of usage:

```
market-bridge
market-bridge -p 8080
market-bridge -l info
market-bridge -h
market-bridge -o file

```

### Third-Party Libraries:

-  Standalone Asio (https://github.com/chriskohlhoff/asio)
- "Lightweight C++ command line option parser (https://github.com/jarro2783/cxxopts)
- "spdlog" for logging (see: https://github.com/gabime/spdlog)
- libssl-dev is required (sudo apt install libssl-dev)


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
