# Copyright 2025 Dennis Hezel
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

cmake_minimum_required(VERSION 3.14...3.27)

include(CMakeDependentOption)

# Hunter package manager options
option(HUNTER_ENABLED "Enable Hunter package manager" off)
option(ASIO_GRPC_HUNTER_BACKEND_BOOST_ASIO "Build Boost.Asio using Hunter" on)
option(ASIO_GRPC_HUNTER_BACKEND_STANDALONE_ASIO "Build standalone Asio using Hunter" off)
if(HUNTER_ENABLED)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcHunter.cmake")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcOptionDefaults.cmake")

# maintainer options
option(ASIO_GRPC_BUILD_TESTS "Build tests" off)
if(ASIO_GRPC_BUILD_TESTS)
    set(ASIO_GRPC_BUILD_EXAMPLES
        on
        CACHE BOOL "Build examples" FORCE)
endif()
option(ASIO_GRPC_DISCOVER_TESTS "Discover tests for ctest" off)
option(ASIO_GRPC_ENABLE_STDEXEC_TESTS "When tests builds are enabled then also create CMake targets for stdexec tests"
       off)
option(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES
       "When tests and/or example builds are enabled then also create CMake targets for C++20"
       ${ASIO_GRPC_DEFAULT_ENABLE_CPP20_TESTS_AND_EXAMPLES})
option(ASIO_GRPC_TEST_CALLBACK_API "Enable tests for the gRPC callback API" on)
cmake_dependent_option(ASIO_GRPC_TEST_CALLBACK_API_CPP20 "Enable tests for the gRPC callback API that required C++20"
                       on "ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES;ASIO_GRPC_TEST_CALLBACK_API" off)

if(ASIO_GRPC_TEST_CALLBACK_API_CPP20)
    list(APPEND VCPKG_MANIFEST_FEATURES "callback-api")
endif()

project(
    asio-grpc
    VERSION 3.5.0
    DESCRIPTION "Asynchronous gRPC with Asio/unified executors"
    HOMEPAGE_URL "https://github.com/Tradias/asio-grpc"
    LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcProjectTopLevel.cmake")
include(GNUInstallDirs)

# user options
option(ASIO_GRPC_INSTALL "Create the install target" ${ASIO_GRPC_PROJECT_IS_TOP_LEVEL})
set(ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    CACHE STRING "Install directory for CMake config files")
option(ASIO_GRPC_BUILD_EXAMPLES "Build examples" off)

# more maintainer options
option(ASIO_GRPC_TEST_COVERAGE "Compile tests with --coverage" off)
set(ASIO_GRPC_COVERAGE_OUTPUT_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/sonarqube-coverage.xml"
    CACHE FILEPATH "Output file for SonarQube XML coverage files")
option(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS "Compile tests with -fsanitize=address,undefined" off)
option(ASIO_GRPC_ENABLE_CHECK_HEADER_SYNTAX_TARGET "Create target to check header files for syntax errors" on)
option(ASIO_GRPC_ENABLE_PKGCONFIG_FALLBACK
       "Fall back to pkg-config when a dependency cannot be found through find_package" on)
option(
    ASIO_GRPC_ENABLE_CMAKE_INSTALL_TEST
    "Add test to ctest that will verify installability of ${PROJECT_NAME} and its CMake function asio_grpc_protobuf_generate"
    on)
set(ASIO_GRPC_CMAKE_INSTALL_TEST_CTEST_COMMAND
    "${CMAKE_CTEST_COMMAND}"
    CACHE FILEPATH "CMake executable used in cmake install test")

set(ASIO_GRPC_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")

if(ASIO_GRPC_BUILD_EXAMPLES)
    # store value of Boost_USE_STATIC_RUNTIME because it gets cleared by find_package(Boost)
    set(ASIO_GRPC_BOOST_USE_STATIC_RUNTIME ${Boost_USE_STATIC_RUNTIME})
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcFindPackages.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcCompileOptions.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcCheckBoostVersion.cmake")
endif()

if(ASIO_GRPC_BUILD_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcInstallGitHooks.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcCoverage.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcCheckStdPmr.cmake")
endif()

if(NOT ASIO_GRPC_PROJECT_IS_TOP_LEVEL OR ASIO_GRPC_BUILD_EXAMPLES)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcProtobufGenerator.cmake")
endif()

add_subdirectory(src)

if(ASIO_GRPC_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(ASIO_GRPC_BUILD_TESTS)
    add_subdirectory(doc)

    find_package(doctest REQUIRED)
    find_package(GTest REQUIRED)
    enable_testing()
    include(doctest)
    add_subdirectory(test)
endif()

if(ASIO_GRPC_INSTALL)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/AsioGrpcInstallation.cmake")
endif()
