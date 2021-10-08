# Copyright 2021 Dennis Hezel
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.19)

project(
    asio-grpc
    VERSION 1.1.1
    DESCRIPTION "Asynchronous gRPC using Boost.Asio's executor model"
    HOMEPAGE_URL "https://github.com/Tradias/asio-grpc"
    LANGUAGES CXX)

option(ASIO_GRPC_INSTALL "Create the install target" on)
option(ASIO_GRPC_BUILD_TESTS "Build tests" off)
option(ASIO_GRPC_BUILD_EXAMPLES "Build examples" off)
option(ASIO_GRPC_DISCOVER_TESTS "Discover tests for ctest" off)
option(ASIO_GRPC_TEST_COVERAGE "Compile tests with --coverage (GCC only)" off)
set(ASIO_GRPC_COVERAGE_WORKING_DIR
    "${CMAKE_BINARY_DIR}"
    CACHE PATH "Working directory when running gcov")
option(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS "Compile tests with -fsanitize=address,undefined" off)
option(ASIO_GRPC_USE_BOOST_CONTAINER "Use Boost.Container instead of <memory_resource>" off)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_subdirectory(src)

if(ASIO_GRPC_BUILD_TESTS OR ASIO_GRPC_BUILD_EXAMPLES)
    find_package(protobuf)
    find_package(gRPC)
    find_package(Boost)
    if(ASIO_GRPC_USE_BOOST_CONTAINER)
        find_package(Boost REQUIRED COMPONENTS coroutine thread container)
    else()
        find_package(Boost REQUIRED COMPONENTS coroutine thread)
    endif()
    include(AsioGrpcCompileOptions)
    include(AsioGrpcCheckBoostVersion)
    include(AsioGrpcInstallGitHooks)
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
