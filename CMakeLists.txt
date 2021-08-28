cmake_minimum_required(VERSION 3.19)

project(
    asio-grpc
    VERSION 1.0.0
    DESCRIPTION "Asynchronous gRPC using Boost.Asio's executor model"
    HOMEPAGE_URL https://github.com/Tradias/asio-grpc
    LANGUAGES CXX)

option(ASIO_GRPC_INSTALL "Create the install target" on)
option(ASIO_GRPC_BUILD_TESTS "Build tests" off)
option(ASIO_GRPC_BUILD_EXAMPLES "Build examples" off)
option(ASIO_GRPC_DISCOVER_TESTS "Discover tests for ctest" off)
option(ASIO_GRPC_TEST_COVERAGE "Compile tests with --coverage (GCC only)" off)
option(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS "Compile tests with -fsanitize=address,undefined" off)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(gRPC)
find_package(Boost)

add_subdirectory(src)

if(ASIO_GRPC_BUILD_TESTS OR ASIO_GRPC_BUILD_EXAMPLES)
    find_package(Boost REQUIRED COMPONENTS coroutine thread)
endif()

if(ASIO_GRPC_BUILD_TESTS)
    find_package(doctest REQUIRED)
    enable_testing()
    include(doctest)
    add_subdirectory(test)
endif()

if(ASIO_GRPC_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
