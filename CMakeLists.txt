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

cmake_minimum_required(VERSION 3.14...3.22)

project(
    asio-grpc
    VERSION 1.3.1
    DESCRIPTION "Asynchronous gRPC using Boost.Asio's executor model"
    HOMEPAGE_URL "https://github.com/Tradias/asio-grpc"
    LANGUAGES CXX)

# user options
option(ASIO_GRPC_INSTALL "Create the install target" on)
option(ASIO_GRPC_USE_BOOST_CONTAINER "Use Boost.Container instead of <memory_resource>" off)

# maintainer options
option(ASIO_GRPC_BUILD_TESTS "Build tests and examples" off)
option(ASIO_GRPC_DISCOVER_TESTS "Discover tests for ctest" off)
option(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES
       "When tests and/or example builds are enabled then also create CMake targets for C++20" on)
option(ASIO_GRPC_TEST_COVERAGE "Compile tests with --coverage (GCC only)" off)
set(ASIO_GRPC_COVERAGE_OUTPUT_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/sonarqube-coverage.xml"
    CACHE PATH "Output file for SonarQube XML coverage files")
option(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS "Compile tests with -fsanitize=address,undefined" off)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(ASIO_GRPC_BUILD_TESTS)
    # store value of Boost_USE_STATIC_RUNTIME because it gets cleared by find_package(Boost)
    set(ASIO_GRPC_BOOST_USE_STATIC_RUNTIME ${Boost_USE_STATIC_RUNTIME})

    find_package(protobuf)
    find_package(gRPC)
    if(ASIO_GRPC_USE_BOOST_CONTAINER)
        find_package(Boost REQUIRED COMPONENTS coroutine thread filesystem container)
    else()
        find_package(Boost REQUIRED COMPONENTS coroutine thread filesystem)
    endif()
    find_package(asio)
    find_package(unifex)
    include(AsioGrpcCompileOptions)
    include(AsioGrpcCheckBoostVersion)
    include(AsioGrpcInstallGitHooks)
    include(AsioGrpcCoverage)
endif()

add_subdirectory(src)

if(ASIO_GRPC_BUILD_TESTS)
    add_subdirectory(example)
    add_subdirectory(doc)

    find_package(doctest REQUIRED)
    enable_testing()
    include(doctest)
    set(ASIO_GRPC_COVERAGE_WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    add_subdirectory(test)
endif()

if(ASIO_GRPC_INSTALL)
    include(AsioGrpcInstallation)
endif()
