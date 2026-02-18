set(test_module g-tests)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/src" tests_src_files)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/f8d7d77c06936315286eb55f8de22cd23c188571.zip
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(${test_module} ${tests_src_files} ${h_files})

target_link_libraries(${test_module} PRIVATE asio
                                             cxxopts
                                             spdlog::spdlog
                                             GTest::gtest_main)

target_compile_definitions(${test_module} PRIVATE
                                APP_NAME="${test_module}"
                                APP_VERSION="1.0.0"
                                DEFAULT_HTTP_PORT=8080)

target_compile_options(${test_module} PRIVATE -Wno-write-strings)
                                                              
target_include_directories(${test_module} PRIVATE "${PROJECT_SOURCE_DIR}/src")

add_test(google-tests ${test_module})

