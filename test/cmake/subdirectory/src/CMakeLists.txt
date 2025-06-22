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

cmake_minimum_required(VERSION 3.14)

project(asio-grpc-cmake-test-subdirectory LANGUAGES CXX)

add_subdirectory(a-g)

find_package(gRPC REQUIRED)
find_package(asio REQUIRED)

# main
add_executable(main)

target_sources(main PRIVATE main.cpp)

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    TARGET main
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/target"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/target.proto")

target_compile_definitions(main INTERFACE $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0A00> # Windows 10
                                          ASIO_NO_DEPRECATED)

target_link_libraries(main PRIVATE asio-grpc::asio-grpc-standalone-asio asio::asio gRPC::grpc++_unsecure)
