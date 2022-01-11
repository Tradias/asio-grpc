# Copyright 2022 Dennis Hezel
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

# example protos
include(AsioGrpcProtobufGenerator)

function(asio_grpc_add_example_protos _asio_grpc_name)
    add_library(asio-grpc-${_asio_grpc_name} OBJECT)

    set(ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR "${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_INCLUDE_DIR}/protos")

    asio_grpc_protobuf_generate(
        GENERATE_GRPC
        TARGET asio-grpc-${_asio_grpc_name}
        OUT_DIR "${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR}"
        PROTOS "${CMAKE_CURRENT_LIST_DIR}/protos/helloworld.proto" "${CMAKE_CURRENT_LIST_DIR}/protos/example.proto")

    target_include_directories(asio-grpc-${_asio_grpc_name}
                               PUBLIC "$<BUILD_INTERFACE:${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_INCLUDE_DIR}>")

    target_link_libraries(asio-grpc-${_asio_grpc_name} PUBLIC protobuf::libprotobuf)

    target_compile_features(asio-grpc-${_asio_grpc_name} PUBLIC cxx_std_17)
endfunction()

# example helper
add_library(asio-grpc-example-helper INTERFACE)

target_link_libraries(asio-grpc-example-helper INTERFACE asio-grpc-common-compile-options)

target_include_directories(asio-grpc-example-helper INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/helper")

# examples
function(asio_grpc_add_example _asio_grpc_name)
    add_executable(asio-grpc-example-${_asio_grpc_name})

    target_sources(asio-grpc-example-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp)

    target_link_libraries(
        asio-grpc-example-${_asio_grpc_name} PRIVATE asio-grpc-example-helper asio-grpc-example-protos-cpp20
                                                     asio-grpc-cpp20-compile-options)
endfunction()

asio_grpc_add_example_protos(example-protos)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_example_protos(example-protos-cpp20)
    target_compile_features(asio-grpc-example-protos-cpp20 PUBLIC cxx_std_20)

    asio_grpc_add_example(hello-world-client)
    target_link_libraries(asio-grpc-example-hello-world-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(hello-world-server)
    target_link_libraries(asio-grpc-example-hello-world-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(streaming-client)
    target_link_libraries(asio-grpc-example-streaming-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(streaming-server)
    target_link_libraries(asio-grpc-example-streaming-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(unifex-server)
    target_link_libraries(asio-grpc-example-unifex-server PRIVATE asio-grpc::asio-grpc-unifex)

    asio_grpc_add_example(unifex-client)
    target_link_libraries(asio-grpc-example-unifex-client PRIVATE asio-grpc::asio-grpc-unifex)
endif()
