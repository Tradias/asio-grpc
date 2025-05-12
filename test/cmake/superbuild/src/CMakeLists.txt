# Copyright 2024 Dennis Hezel
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

if(TEST_CALLBACK_API_CPP20)
    list(APPEND VCPKG_MANIFEST_FEATURES "callback-api")
endif()

project(asio-grpc-cmake-test LANGUAGES CXX)

find_package(
    asio-grpc
    CONFIG
    REQUIRED
    NO_CMAKE_PACKAGE_REGISTRY
    NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH)

set(Boost_NO_SYSTEM_PATHS on)
find_package(Boost REQUIRED)

find_package(asio REQUIRED)

# common compile options
add_library(compile-options INTERFACE)

target_compile_definitions(compile-options INTERFACE $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0A00> # Windows 10
                                                     BOOST_ASIO_NO_DEPRECATED ASIO_NO_DEPRECATED)

function(create_object_library _name)
    add_library(${_name} OBJECT)

    target_sources(${_name} PRIVATE ${ARGN})

    target_link_libraries(${_name} PRIVATE compile-options)
endfunction()

# TARGET option
create_object_library(target-option target.cpp)

target_link_libraries(target-option PRIVATE asio-grpc::asio-grpc-standalone-asio asio::asio)

# /* [asio_grpc_protobuf_generate-example] */
asio_grpc_protobuf_generate(
    GENERATE_GRPC GENERATE_MOCK_CODE
    TARGET target-option
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/target"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/target.proto")
# /* [asio_grpc_protobuf_generate-example] */

# OUT_VAR option
set(OUT_VAR_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/out_var/")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR OUT_VAR_GENERATED_SOURCES
    OUT_DIR "${OUT_VAR_GENERATED_PROTOS_OUT_DIR}"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/out_var/v1/out_var.proto"
           "${CMAKE_CURRENT_SOURCE_DIR}/proto/out_var/msg/message.proto"
           "${CMAKE_CURRENT_SOURCE_DIR}/proto/out_var/subdir/other.1.proto")

create_object_library(out-var-option out_var.cpp ${OUT_VAR_GENERATED_SOURCES})

target_link_libraries(out-var-option PRIVATE asio-grpc::asio-grpc Boost::boost)

target_include_directories(out-var-option PRIVATE "${OUT_VAR_GENERATED_PROTOS_OUT_DIR}")

# DESCRIPTOR option
create_object_library(descriptor-option descriptor.cpp)

target_link_libraries(descriptor-option PRIVATE asio-grpc::asio-grpc Boost::boost)

set(DESCRIPTOR_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/descriptor")

asio_grpc_protobuf_generate(
    GENERATE_DESCRIPTORS
    TARGET descriptor-option
    OUT_DIR "${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/descriptor.proto")

target_include_directories(descriptor-option PRIVATE "${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}")

target_compile_definitions(descriptor-option
                           PRIVATE "DESCRIPTOR_FILE=R\"(${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}/descriptor.desc)\"")

# main
add_executable(main)

target_sources(main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(main PRIVATE target-option out-var-option descriptor-option compile-options)
