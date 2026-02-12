# $\color{MidnightBlue}\textit{\textbf{market-bridge}}$


![C++](https://img.shields.io/badge/C++-17-purple?logo=C++)
![asio](https://img.shields.io/badge/asio-1.36.0-lightblue?logo=asio)
![cmake](https://img.shields.io/badge/cmake-3.30-brightgreen)
![Ubuntu](https://img.shields.io/badge/Ubuntu-18.04+-red?logo=Ubuntu)


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
-p, --port arg       specify port (default: 8080)
-l, --log_level arg  specify log level (error, warning, trace, debug, 
                       critical, off) (default: info)
-h, --help           print usage

```

### Samples of usage:

```
market-bridge
market-bridge -p 8080
market-bridge -l info
market-bridge -h

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




