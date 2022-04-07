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

# example protos
add_library(asio-grpc-example-protos OBJECT)

include(AsioGrpcProtobufGenerator)
asio_grpc_protobuf_generate(
    GENERATE_GRPC
    TARGET asio-grpc-example-protos
    USAGE_REQUIREMENT PUBLIC
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto"
    PROTOS "${CMAKE_CURRENT_LIST_DIR}/proto/helloworld/helloworld.proto"
           "${CMAKE_CURRENT_LIST_DIR}/proto/example/v1/example.proto"
           "${CMAKE_CURRENT_LIST_DIR}/proto/example/v1/exampleExt.proto")

target_link_libraries(asio-grpc-example-protos PUBLIC protobuf::libprotobuf)

target_compile_features(asio-grpc-example-protos PUBLIC cxx_std_17)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    add_library(asio-grpc-example-protos-cpp20 OBJECT)
    target_sources(asio-grpc-example-protos-cpp20 PRIVATE $<TARGET_PROPERTY:asio-grpc-test-protobuf,SOURCES>)
    target_include_directories(asio-grpc-example-protos-cpp20
                               PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")
    target_link_libraries(asio-grpc-example-protos-cpp20 PUBLIC protobuf::libprotobuf)
    target_compile_features(asio-grpc-example-protos-cpp20 PUBLIC cxx_std_20)

    asio_grpc_add_example(hello-world-client)
    target_link_libraries(asio-grpc-example-hello-world-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(hello-world-server)
    target_link_libraries(asio-grpc-example-hello-world-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(streaming-client)
    target_link_libraries(asio-grpc-example-streaming-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(streaming-server)
    target_link_libraries(asio-grpc-example-streaming-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(share-io-context-client)
    target_link_libraries(asio-grpc-example-share-io-context-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(share-io-context-server)
    target_link_libraries(asio-grpc-example-share-io-context-server PRIVATE asio-grpc::asio-grpc)

    if(ASIO_GRPC_ENABLE_IO_URING_EXAMPLES OR ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        asio_grpc_add_example(file-transfer-client)
        target_link_libraries(asio-grpc-example-file-transfer-client
                              PRIVATE asio-grpc::asio-grpc $<$<PLATFORM_ID:Linux>:PkgConfig::liburing>)
        target_compile_definitions(asio-grpc-example-file-transfer-client
                                   PRIVATE $<$<PLATFORM_ID:Linux>:BOOST_ASIO_HAS_IO_URING BOOST_ASIO_DISABLE_EPOLL>)

        asio_grpc_add_example(file-transfer-server)
        target_link_libraries(asio-grpc-example-file-transfer-server
                              PRIVATE asio-grpc::asio-grpc $<$<PLATFORM_ID:Linux>:PkgConfig::liburing>)
        target_compile_definitions(asio-grpc-example-file-transfer-server
                                   PRIVATE $<$<PLATFORM_ID:Linux>:BOOST_ASIO_HAS_IO_URING BOOST_ASIO_DISABLE_EPOLL>)
    endif()

    asio_grpc_add_example(unifex-server)
    target_link_libraries(asio-grpc-example-unifex-server PRIVATE asio-grpc::asio-grpc-unifex)

    asio_grpc_add_example(unifex-client)
    target_link_libraries(asio-grpc-example-unifex-client PRIVATE asio-grpc::asio-grpc-unifex)
endif()

add_subdirectory(snippets)
