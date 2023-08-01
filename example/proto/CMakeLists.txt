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
    OUT_VAR "ASIO_GRPC_EXAMPLE_PROTO_SOURCES"
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}"
    PROTOS "${CMAKE_CURRENT_LIST_DIR}/helloworld/helloworld.proto"
           "${CMAKE_CURRENT_LIST_DIR}/grpc/health/v1/health.proto" "${CMAKE_CURRENT_LIST_DIR}/example/v1/example.proto"
           "${CMAKE_CURRENT_LIST_DIR}/example/v1/example_ext.proto")

function(asio_grpc_add_example_proto_target _asio_grpc_name)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(${_asio_grpc_name} PRIVATE ${ASIO_GRPC_EXAMPLE_PROTO_SOURCES})

    target_link_libraries(${_asio_grpc_name} PUBLIC protobuf::libprotobuf asio-grpc-compile-options)

    target_include_directories(${_asio_grpc_name} SYSTEM
                               PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")
endfunction()

asio_grpc_add_example_proto_target(asio-grpc-example-protos)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_example_proto_target(asio-grpc-example-protos-cpp20)
    target_link_libraries(asio-grpc-example-protos-cpp20 PUBLIC asio-grpc-compile-options-cpp20)
endif()
