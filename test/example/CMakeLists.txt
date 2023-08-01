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

add_executable(asio-grpc-test-examples)

target_sources(asio-grpc-test-examples PRIVATE "test_examples.cpp")

target_link_libraries(asio-grpc-test-examples PRIVATE asio-grpc-test-main Boost::filesystem)

add_dependencies(
    asio-grpc-test-examples
    asio-grpc-example-streaming-client
    asio-grpc-example-streaming-server
    asio-grpc-example-share-io-context-client
    asio-grpc-example-share-io-context-server
    asio-grpc-example-hello-world-client
    asio-grpc-example-hello-world-server
    asio-grpc-example-generic-client
    asio-grpc-example-generic-server
    asio-grpc-example-multi-threaded-client
    asio-grpc-example-multi-threaded-server
    asio-grpc-example-client-rpc
    asio-grpc-example-unifex-client
    asio-grpc-example-unifex-server)

set(ASIO_GRPC_TEMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/temp")
file(MAKE_DIRECTORY "${ASIO_GRPC_TEMP_DIR}")

target_compile_definitions(
    asio-grpc-test-examples
    PRIVATE "ASIO_GRPC_EXAMPLE_TEMP_DIR=\"${ASIO_GRPC_TEMP_DIR}\""
            "ASIO_GRPC_EXAMPLE_STREAMING_CLIENT=\"$<TARGET_FILE:asio-grpc-example-streaming-client>\""
            "ASIO_GRPC_EXAMPLE_STREAMING_SERVER=\"$<TARGET_FILE:asio-grpc-example-streaming-server>\""
            "ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_CLIENT=\"$<TARGET_FILE:asio-grpc-example-share-io-context-client>\""
            "ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_SERVER=\"$<TARGET_FILE:asio-grpc-example-share-io-context-server>\""
            "ASIO_GRPC_EXAMPLE_HELLO_WORLD_CLIENT=\"$<TARGET_FILE:asio-grpc-example-hello-world-client>\""
            "ASIO_GRPC_EXAMPLE_HELLO_WORLD_SERVER=\"$<TARGET_FILE:asio-grpc-example-hello-world-server>\""
            "ASIO_GRPC_EXAMPLE_GENERIC_CLIENT=\"$<TARGET_FILE:asio-grpc-example-generic-client>\""
            "ASIO_GRPC_EXAMPLE_GENERIC_SERVER=\"$<TARGET_FILE:asio-grpc-example-generic-server>\""
            "ASIO_GRPC_EXAMPLE_MULTI_THREADED_CLIENT=\"$<TARGET_FILE:asio-grpc-example-multi-threaded-client>\""
            "ASIO_GRPC_EXAMPLE_MULTI_THREADED_SERVER=\"$<TARGET_FILE:asio-grpc-example-multi-threaded-server>\""
            "ASIO_GRPC_EXAMPLE_HIGH_LEVEL_CLIENT=\"$<TARGET_FILE:asio-grpc-example-client-rpc>\""
            "ASIO_GRPC_EXAMPLE_UNIFEX_CLIENT=\"$<TARGET_FILE:asio-grpc-example-unifex-client>\""
            "ASIO_GRPC_EXAMPLE_UNIFEX_SERVER=\"$<TARGET_FILE:asio-grpc-example-unifex-server>\"")

if(ASIO_GRPC_ENABLE_IO_URING_EXAMPLES OR ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_dependencies(asio-grpc-test-examples asio-grpc-example-file-transfer-client
                     asio-grpc-example-file-transfer-server)

    target_compile_definitions(
        asio-grpc-test-examples
        PRIVATE "ASIO_GRPC_EXAMPLE_FILE_TRANSFER_CLIENT=\"$<TARGET_FILE:asio-grpc-example-file-transfer-client>\""
                "ASIO_GRPC_EXAMPLE_FILE_TRANSFER_SERVER=\"$<TARGET_FILE:asio-grpc-example-file-transfer-server>\"")
endif()

if(ASIO_GRPC_DISCOVER_TESTS)
    doctest_discover_tests(asio-grpc-test-examples)
endif()
