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

if(ASIO_GRPC_TEST_COVERAGE)
    include(AsioGrpcCoverage)
endif()

# asio-grpc objects
add_library(asio-grpc-objects OBJECT)

get_target_property(ASIO_GRPC_SOURCE_FILES asio-grpc ASIO_GRPC_SOURCE_FILES)
target_sources(asio-grpc-objects PRIVATE ${ASIO_GRPC_SOURCE_FILES})
unset(ASIO_GRPC_SOURCE_FILES)

target_link_libraries(asio-grpc-objects PUBLIC asio-grpc-common-compile-options)

if(ASIO_GRPC_TEST_COVERAGE)
    asio_grpc_add_coverage_flags(asio-grpc-objects)
endif()

# grpc generate
include(AsioGrpcProtobufGenerator)
set(ASIO_GRPC_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR
    ASIO_GRPC_GENERATED_SOURCES
    PROTOC_OUT_DIR
    "${ASIO_GRPC_GENERATED_PROTOS_OUT_DIR}"
    PROTOS
    "${CMAKE_CURRENT_LIST_DIR}/protos/test.proto")

# tests
if(ASIO_GRPC_USE_BOOST_CONTAINER)
    set(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE "#include <boost/container/pmr/monotonic_buffer_resource.hpp>")
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/utils/memoryResource.hpp.in"
               "${CMAKE_CURRENT_SOURCE_DIR}/utils/memoryResource.hpp" @ONLY)

function(asio_grpc_add_test _asio-grpc_name)
    add_executable(${_asio-grpc_name})

    target_sources(
        ${_asio-grpc_name}
        PRIVATE main.cpp
                $<TARGET_PROPERTY:asio-grpc,ASIO_GRPC_HEADER_FILES>
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/asioUtils.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcClientServerTest.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcClientServerTest.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcContextTest.hpp"
                "${ASIO_GRPC_GENERATED_SOURCES}"
                ${ARGN})

    if(ASIO_GRPC_TEST_COVERAGE)
        asio_grpc_add_coverage_flags(${_asio-grpc_name})
    endif()

    target_link_libraries(${_asio-grpc_name} PRIVATE asio-grpc-objects doctest::doctest Boost::coroutine Boost::thread)

    target_include_directories(
        ${_asio-grpc_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${ASIO_GRPC_GENERATED_PROTOS_OUT_DIR}> $<INSTALL_INTERFACE:include>)

    target_precompile_headers(${_asio-grpc_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utils/precompiledHeader.hpp")
endfunction()

set(ASIO_GRPC_TEST_SOURCE_FILES "test-asio-grpc.cpp")

asio_grpc_add_test(asio-grpc-test-cpp17 ${ASIO_GRPC_TEST_SOURCE_FILES})
target_compile_definitions(asio-grpc-test-cpp17 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"C++17\"")

asio_grpc_add_test(asio-grpc-test-cpp20 ${ASIO_GRPC_TEST_SOURCE_FILES} "test-asio-grpc-20.cpp")
target_compile_definitions(asio-grpc-test-cpp20 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"C++20\"")
target_link_libraries(asio-grpc-test-cpp20 PRIVATE asio-grpc-cpp20-compile-options)

unset(ASIO_GRPC_TEST_SOURCE_FILES)

if(ASIO_GRPC_DISCOVER_TESTS)
    doctest_discover_tests(
        asio-grpc-test-cpp17 # workaround for regression in doctest 2.4.6
        #
        # if given arguments:
        #
        # "EQUAL" "1"
        #
        # Unknown arguments specified
        ADD_LABELS
        0)
    doctest_discover_tests(asio-grpc-test-cpp20 ADD_LABELS 0)
endif()

if(ASIO_GRPC_TEST_COVERAGE)
    asio_grpc_coverage_report_for_target(asio-grpc-test-cpp20 "${CMAKE_CURRENT_SOURCE_DIR}/test-asio-grpc.cpp")
endif()
