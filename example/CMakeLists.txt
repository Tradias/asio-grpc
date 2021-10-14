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

function(asio_grpc_add_example_protos _asio_grpc_name)
    include(AsioGrpcProtobufGenerator)

    set(ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

    asio_grpc_protobuf_generate(
        GENERATE_GRPC
        OUT_VAR ASIO_GRPC_GENERATED_EXAMPLE_SOURCES
        PROTOC_OUT_DIR "${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR}"
        PROTOS "${CMAKE_CURRENT_LIST_DIR}/protos/helloworld.proto" "${CMAKE_CURRENT_LIST_DIR}/protos/example.proto")

    add_library(asio-grpc-${_asio_grpc_name} OBJECT)

    target_sources(asio-grpc-${_asio_grpc_name} PRIVATE ${ASIO_GRPC_GENERATED_EXAMPLE_SOURCES})

    target_include_directories(
        asio-grpc-${_asio_grpc_name} PUBLIC $<BUILD_INTERFACE:${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR}>
                                            $<INSTALL_INTERFACE:include>)

    target_link_libraries(asio-grpc-${_asio_grpc_name} PRIVATE asio-grpc-common-compile-options)
endfunction()

function(asio_grpc_add_example _asio_grpc_name)
    add_executable(asio-grpc-${_asio_grpc_name})

    target_sources(asio-grpc-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp ${ASIO_GRPC_GENERATED_EXAMPLE_SOURCES})

    target_link_libraries(asio-grpc-${_asio_grpc_name} PRIVATE asio-grpc-common-compile-options)
endfunction()

asio_grpc_add_example_protos(example-protos)
asio_grpc_add_example(example-server)
target_link_libraries(asio-grpc-example-server PRIVATE Boost::coroutine Boost::thread asio-grpc-example-protos)

asio_grpc_add_example(example-client)
target_link_libraries(asio-grpc-example-client PRIVATE Boost::coroutine Boost::thread asio-grpc-example-protos)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_example(blog)
    target_link_libraries(asio-grpc-blog PRIVATE asio-grpc-cpp20-compile-options asio-grpc-example-protos)

    asio_grpc_add_example_protos(example-protos-cpp20)
    target_link_libraries(asio-grpc-example-protos-cpp20 PRIVATE asio-grpc-cpp20-compile-options)

    asio_grpc_add_example(hello-world-server-cpp20)
    target_link_libraries(asio-grpc-hello-world-server-cpp20 PRIVATE asio-grpc-example-protos-cpp20
                                                                     asio-grpc-cpp20-compile-options)

    asio_grpc_add_example(hello-world-client-cpp20)
    target_link_libraries(asio-grpc-hello-world-client-cpp20 PRIVATE asio-grpc-example-protos-cpp20
                                                                     asio-grpc-cpp20-compile-options)
endif()
