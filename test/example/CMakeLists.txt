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

add_executable(asio-grpc-test-examples)

target_sources(asio-grpc-test-examples PRIVATE "test_examples.cpp")

target_link_libraries(asio-grpc-test-examples PRIVATE asio-grpc-compile-options asio-grpc-test-util-boost-asio
                                                      asio-grpc-test-main asio-grpc-test-protos Boost::process)

target_precompile_headers(asio-grpc-test-examples REUSE_FROM "asio-grpc-test-util-boost-asio")

add_dependencies(
    asio-grpc-test-examples
    asio-grpc-example-async-generator-client
    asio-grpc-example-async-generator-server
    asio-grpc-example-streaming-client
    asio-grpc-example-streaming-server
    asio-grpc-example-share-io-context-client
    asio-grpc-example-share-io-context-server
    asio-grpc-example-main-io-context-server
    asio-grpc-example-hello-world-client
    asio-grpc-example-hello-world-server
    asio-grpc-example-hello-world-server-arena
    asio-grpc-example-generic-client
    asio-grpc-example-generic-server
    asio-grpc-example-multi-threaded-client
    asio-grpc-example-multi-threaded-server
    asio-grpc-example-multi-threaded-alternative-client
    asio-grpc-example-multi-threaded-alternative-server
    asio-grpc-example-unifex-client
    asio-grpc-example-unifex-server)

set(ASIO_GRPC_TEMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/temp")
file(MAKE_DIRECTORY "${ASIO_GRPC_TEMP_DIR}")

target_compile_definitions(
    asio-grpc-test-examples
    PRIVATE
        "ASIO_GRPC_EXAMPLE_TEMP_DIR=\"${ASIO_GRPC_TEMP_DIR}\""
        "ASIO_GRPC_EXAMPLE_ASYNC_GENERATOR_CLIENT=\"$<TARGET_FILE:asio-grpc-example-async-generator-client>\""
        "ASIO_GRPC_EXAMPLE_ASYNC_GENERATOR_SERVER=\"$<TARGET_FILE:asio-grpc-example-async-generator-server>\""
        "ASIO_GRPC_EXAMPLE_STREAMING_CLIENT=\"$<TARGET_FILE:asio-grpc-example-streaming-client>\""
        "ASIO_GRPC_EXAMPLE_STREAMING_SERVER=\"$<TARGET_FILE:asio-grpc-example-streaming-server>\""
        "ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_CLIENT=\"$<TARGET_FILE:asio-grpc-example-share-io-context-client>\""
        "ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_SERVER=\"$<TARGET_FILE:asio-grpc-example-share-io-context-server>\""
        "ASIO_GRPC_EXAMPLE_MAIN_IO_CONTEXT_SERVER=\"$<TARGET_FILE:asio-grpc-example-main-io-context-server>\""
        "ASIO_GRPC_EXAMPLE_HELLO_WORLD_CLIENT=\"$<TARGET_FILE:asio-grpc-example-hello-world-client>\""
        "ASIO_GRPC_EXAMPLE_HELLO_WORLD_SERVER=\"$<TARGET_FILE:asio-grpc-example-hello-world-server>\""
        "ASIO_GRPC_EXAMPLE_HELLO_WORLD_SERVER_ARENA=\"$<TARGET_FILE:asio-grpc-example-hello-world-server-arena>\""
        "ASIO_GRPC_EXAMPLE_GENERIC_CLIENT=\"$<TARGET_FILE:asio-grpc-example-generic-client>\""
        "ASIO_GRPC_EXAMPLE_GENERIC_SERVER=\"$<TARGET_FILE:asio-grpc-example-generic-server>\""
        "ASIO_GRPC_EXAMPLE_MULTI_THREADED_CLIENT=\"$<TARGET_FILE:asio-grpc-example-multi-threaded-client>\""
        "ASIO_GRPC_EXAMPLE_MULTI_THREADED_SERVER=\"$<TARGET_FILE:asio-grpc-example-multi-threaded-server>\""
        "ASIO_GRPC_EXAMPLE_MULTI_THREADED_ALTERNATIVE_CLIENT=\"$<TARGET_FILE:asio-grpc-example-multi-threaded-alternative-client>\""
        "ASIO_GRPC_EXAMPLE_MULTI_THREADED_ALTERNATIVE_SERVER=\"$<TARGET_FILE:asio-grpc-example-multi-threaded-alternative-server>\""
        "ASIO_GRPC_EXAMPLE_UNIFEX_CLIENT=\"$<TARGET_FILE:asio-grpc-example-unifex-client>\""
        "ASIO_GRPC_EXAMPLE_UNIFEX_SERVER=\"$<TARGET_FILE:asio-grpc-example-unifex-server>\"")

if(ASIO_GRPC_ENABLE_IO_URING_EXAMPLES OR ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_dependencies(asio-grpc-test-examples asio-grpc-example-file-transfer-client
                     asio-grpc-example-file-transfer-server)

    target_compile_definitions(
        asio-grpc-test-examples
        PRIVATE BOOST_PROCESS_USE_STD_FS
                "ASIO_GRPC_EXAMPLE_FILE_TRANSFER_CLIENT=\"$<TARGET_FILE:asio-grpc-example-file-transfer-client>\""
                "ASIO_GRPC_EXAMPLE_FILE_TRANSFER_SERVER=\"$<TARGET_FILE:asio-grpc-example-file-transfer-server>\"")
endif()

if(ASIO_GRPC_DISCOVER_TESTS)
    doctest_discover_tests(asio-grpc-test-examples)
endif()
