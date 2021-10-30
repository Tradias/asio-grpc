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

project(asio-grpc-cmake-test LANGUAGES CXX)

find_package(Boost REQUIRED coroutine thread container)
find_package(
    gRPC
    CONFIG
    REQUIRED
    NO_CMAKE_PACKAGE_REGISTRY
    NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH)

find_package(
    asio-grpc
    CONFIG
    REQUIRED
    NO_CMAKE_PACKAGE_REGISTRY
    NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH)

# common compile options
add_library(compile-options INTERFACE)

target_link_libraries(
    compile-options
    INTERFACE asio-grpc::asio-grpc
              gRPC::grpc++
              Boost::headers
              Boost::coroutine
              Boost::thread
              ${Boost_CONTAINER_LIBRARY}
              Boost::disable_autolinking)

target_compile_definitions(
    compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:
              BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
              BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT
              _WIN32_WINNT=0x0A00 # Windows 10
              WINVER=0x0A00>
              BOOST_ASIO_NO_DEPRECATED)

function(create_object_library _name)
    add_library(${_name} OBJECT)

    target_sources(${_name} PRIVATE ${ARGN})

    target_link_libraries(${_name} PRIVATE compile-options)
endfunction()

# TARGET option
create_object_library(target-option target.cpp)

set(TARGET_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    TARGET target-option
    OUT_DIR "${TARGET_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/target.proto")

target_include_directories(target-option PRIVATE "${TARGET_GENERATED_PROTOS_OUT_DIR}")

# OUT_VAR option
set(OUT_VAR_GENERATED_PROTOS_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/outVar/")
set(OUT_VAR_GENERATED_PROTOS_OUT_DIR "${OUT_VAR_GENERATED_PROTOS_INCLUDE_DIR}/protos")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR OUT_VAR_GENERATED_SOURCES
    OUT_DIR "${OUT_VAR_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/outVar.proto")

create_object_library(out-var-option outVar.cpp ${OUT_VAR_GENERATED_SOURCES})

target_include_directories(out-var-option PRIVATE "${OUT_VAR_GENERATED_PROTOS_INCLUDE_DIR}")

# DESCRIPTOR option
create_object_library(descriptor-option descriptor.cpp)

set(DESCRIPTOR_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/descriptor")

asio_grpc_protobuf_generate(
    GENERATE_GRPC GENERATE_DESCRIPTORS
    TARGET descriptor-option
    OUT_DIR "${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/descriptor.proto")

target_include_directories(descriptor-option PRIVATE "${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}")

target_compile_definitions(descriptor-option
                           PRIVATE "DESCRIPTOR_FILE=R\"(${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}/descriptor.desc)\"")

# main
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE target-option out-var-option descriptor-option compile-options)
