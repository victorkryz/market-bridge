cmake_minimum_required(VERSION 3.16)

set(app market-bridge)
project(${app})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    # Pick a specific tag or commit:
    GIT_TAG asio-1-36-0
)

FetchContent_GetProperties(asio)
if (NOT asio_POPULATED)
    FetchContent_MakeAvailable(asio)

    # Create a header-only target
    add_library(asio INTERFACE)

    target_include_directories(asio INTERFACE
        ${asio_SOURCE_DIR}/asio/include     # path with <asio.hpp>
    )

    target_compile_definitions(asio INTERFACE
        ASIO_STANDALONE
    )

    # POSIX threading if you use threads
    if (UNIX AND NOT APPLE)
        target_link_libraries(asio INTERFACE pthread)
    endif()
endif()


set(SPDLOG_FMT_EXTERNAL OFF)
set(SPDLOG_BUILD_EXAMPLE OFF)
set(SPDLOG_BUILD_TESTS   OFF)
set(SPDLOG_BUILD_BENCH  OFF)
set(SPDLOG_INSTALL      OFF)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.0
)

FetchContent_GetProperties(spdlog)
if (NOT spdlog_POPULATED)
    FetchContent_MakeAvailable(spdlog)
endif()


FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        v3.2.1  
)

FetchContent_GetProperties(cxxopts)
if (NOT cxxopts_POPULATED)
    FetchContent_MakeAvailable(cxxopts)
endif()


# Threads (needed on most POSIX systems)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

file(GLOB SRC_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp src/*.h src/*.hpp src/*.c)

add_executable(${app} ${SRC_FILES})

target_include_directories(${app} PRIVATE
                                  ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(${app} PRIVATE asio
                                     cxxopts
                                     spdlog::spdlog   
                                     OpenSSL::SSL
                                     OpenSSL::Crypto)

target_compile_definitions(${app} PRIVATE
                            APP_NAME="${app}"
                            APP_VERSION="1.0.0"
)
