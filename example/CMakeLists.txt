# Copyright 2025 Dennis Hezel
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

target_include_directories(asio-grpc-example-helper INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/helper")

# example protos
add_subdirectory(proto)

# examples
function(asio_grpc_add_example _asio_grpc_name _asio_grpc_cxx_standard)
    add_executable(asio-grpc-example-${_asio_grpc_name})

    target_sources(asio-grpc-example-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp)

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(
        asio-grpc-example-${_asio_grpc_name}
        PRIVATE asio-grpc-example-helper asio-grpc-compile-options${_asio_grpc_cxx_standard}
                asio-grpc-example-protos${_asio_grpc_cxx_standard})

    target_compile_options(
        asio-grpc-example-${_asio_grpc_name}
        PRIVATE $<$<OR:$<CXX_COMPILER_ID:GNU,AppleClang>,$<STREQUAL:${CMAKE_CXX_COMPILER_FRONTEND_VARIANT},GNU>>:
                -Wsign-conversion -Wshadow>)
endfunction()

asio_grpc_add_example(generic-server "17")
target_link_libraries(asio-grpc-example-generic-server PRIVATE asio-grpc::asio-grpc Boost::coroutine)

asio_grpc_add_example(generic-client "17")
target_link_libraries(asio-grpc-example-generic-client PRIVATE asio-grpc::asio-grpc Boost::coroutine)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_example(hello-world-client "20")
    target_link_libraries(asio-grpc-example-hello-world-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(hello-world-server "20")
    target_link_libraries(asio-grpc-example-hello-world-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(hello-world-server-arena "20")
    target_link_libraries(asio-grpc-example-hello-world-server-arena PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(streaming-client "20")
    target_link_libraries(asio-grpc-example-streaming-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(streaming-server "20")
    target_link_libraries(asio-grpc-example-streaming-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(share-io-context-client "20")
    target_link_libraries(asio-grpc-example-share-io-context-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(share-io-context-server "20")
    target_link_libraries(asio-grpc-example-share-io-context-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(main-io-context-server "20")
    target_link_libraries(asio-grpc-example-main-io-context-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(multi-threaded-client "20")
    target_link_libraries(asio-grpc-example-multi-threaded-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(multi-threaded-alternative-client "20")
    target_link_libraries(asio-grpc-example-multi-threaded-alternative-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(multi-threaded-server "20")
    target_link_libraries(asio-grpc-example-multi-threaded-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(multi-threaded-alternative-server "20")
    target_link_libraries(asio-grpc-example-multi-threaded-alternative-server PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(async-generator-client "20")
    target_link_libraries(asio-grpc-example-async-generator-client PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_example(async-generator-server "20")
    target_link_libraries(asio-grpc-example-async-generator-server PRIVATE asio-grpc::asio-grpc)

    if(TARGET unifex::unifex)
        asio_grpc_add_example(unifex-server "20")
        target_link_libraries(asio-grpc-example-unifex-server PRIVATE asio-grpc::asio-grpc-unifex)

        asio_grpc_add_example(unifex-client "20")
        target_link_libraries(asio-grpc-example-unifex-client PRIVATE asio-grpc::asio-grpc-unifex)
    endif()
endif()

# documentation snippets
if(ASIO_GRPC_BUILD_TESTS)
    add_subdirectory(snippets)
endif()
