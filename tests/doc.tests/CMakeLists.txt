set(test_module doc-tests)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/src" tests_src_files)

add_executable(${test_module} ${tests_src_files} ${h_files})

target_link_libraries(${test_module} PRIVATE asio
    cxxopts
    spdlog::spdlog)

target_compile_definitions(${test_module} PRIVATE
    APP_NAME="${test_module}"
    APP_VERSION="1.0.0"
    DEFAULT_HTTP_PORT=8080)


if(UNIX)
    target_compile_options(${test_module} PRIVATE -Wno-write-strings)
endif()

target_include_directories(${test_module} PRIVATE "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/tests/thirdparty/doctest")

add_test(doc-tests ${test_module})


