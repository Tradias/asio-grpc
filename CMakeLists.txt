cmake_minimum_required(VERSION 3.15)

project(
    asio-grpc
    VERSION 0.1
    DESCRIPTION "Asio execution_context based on gRPC completion queue"
    HOMEPAGE_URL https://github.com/Tradias/asio-grpc
    LANGUAGES CXX)

option(ASIO_GRPC_INSTALL on)
option(ASIO_GRPC_BUILD_TESTS off)
option(ASIO_GRPC_BUILD_DOCUMENTATION off)
option(ASIO_GRPC_DISCOVER_TESTS off)
option(ASIO_GRPC_TEST_COVERAGE off)
option(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS off)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(gRPC REQUIRED)
find_package(Boost REQUIRED COMPONENTS coroutine thread)

add_subdirectory(src)

if(ASIO_GRPC_BUILD_TESTS)
    find_package(doctest REQUIRED)
    enable_testing()
    include(doctest)
    add_subdirectory(tests)
endif()

# if(ASIO_GRPC_BUILD_DOCUMENTATION) add_subdirectory(doc) endif()
