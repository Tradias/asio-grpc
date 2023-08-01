# Copyright 2023 Dennis Hezel
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

asio_grpc_protobuf_generate(
    GENERATE_GRPC GENERATE_MOCK_CODE
    OUT_VAR "ASIO_GRPC_TEST_PROTO_SOURCES"
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/grpc/health/v1/health.proto" "${CMAKE_CURRENT_SOURCE_DIR}/test/v1/test.proto"
           "${CMAKE_CURRENT_SOURCE_DIR}/test/msg/message.proto")

function(asio_grpc_add_test_proto_target _asio_grpc_name _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(${_asio_grpc_name} PRIVATE ${ASIO_GRPC_TEST_PROTO_SOURCES})

    target_link_libraries(${_asio_grpc_name} PUBLIC protobuf::libprotobuf)

    target_include_directories(${_asio_grpc_name} SYSTEM
                               PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(${_asio_grpc_name} PRIVATE asio-grpc-compile-options${_asio_grpc_cxx_standard}
                                                     asio-grpc-coverage-options)
endfunction()

asio_grpc_add_test_proto_target(asio-grpc-test-protos "17")
asio_grpc_add_test_proto_target(asio-grpc-test-protos-cpp20 "20")
