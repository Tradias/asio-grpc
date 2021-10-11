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

set(ASIO_GRPC_TEST_PROTOS
    ""
    CACHE FILEPATH "Path to .proto files")

set(Boost_NO_SYSTEM_PATHS on)
set(Boost_USE_STATIC_RUNTIME on)
find_package(Boost CONFIG REQUIRED NO_CMAKE_SYSTEM_PATH COMPONENTS coroutine thread)
find_package(gRPC NO_CMAKE_SYSTEM_PATH)
find_package(asio-grpc CONFIG NO_CMAKE_SYSTEM_PATH)

if(NOT TARGET protobuf::protoc)
    add_executable(protobuf::protoc IMPORTED)

    set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}")
endif()

set(ASIO_GRPC_CMAKE_TEST_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR
    ASIO_GRPC_CMAKE_TEST_GENERATED_SOURCES
    PROTOC_OUT_DIR
    "${ASIO_GRPC_CMAKE_TEST_GENERATED_PROTOS_OUT_DIR}"
    PROTOS
    "${ASIO_GRPC_TEST_PROTOS}")

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
                                       "${ASIO_GRPC_CMAKE_TEST_GENERATED_SOURCES}")

target_link_libraries(${PROJECT_NAME} PUBLIC asio-grpc::asio-grpc gRPC::grpc++ Boost::headers Boost::coroutine
                                             Boost::thread Boost::disable_autolinking)

target_include_directories(${PROJECT_NAME} PRIVATE "${ASIO_GRPC_CMAKE_TEST_GENERATED_PROTOS_OUT_DIR}")

target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE $<$<CXX_COMPILER_ID:MSVC>:
            BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
            BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
            BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
            BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
            BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT
            _WIN32_WINNT=0x0A00 # Windows 10
            WINVER=0x0A00>
            BOOST_ASIO_NO_DEPRECATED)

install(TARGETS ${PROJECT_NAME} RUNTIME)
