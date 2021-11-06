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

target_link_libraries(asio-grpc-objects PUBLIC asio-grpc-sources asio-grpc-common-compile-options asio-grpc
                                               Boost::headers)

# grpc generate
include(AsioGrpcProtobufGenerator)

set(ASIO_GRPC_GENERATED_PROTOS_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(ASIO_GRPC_GENERATED_PROTOS_OUT_DIR "${ASIO_GRPC_GENERATED_PROTOS_INCLUDE_DIR}/protos")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR ASIO_GRPC_GENERATED_SOURCES
    OUT_DIR "${ASIO_GRPC_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/protos/test.proto")

# tests
if(ASIO_GRPC_USE_BOOST_CONTAINER)
    set(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE [[#include <boost/container/pmr/monotonic_buffer_resource.hpp>
    #include <boost/container/pmr/global_resource.hpp>]])
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/utils/memoryResource.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/utils/memoryResource.hpp" @ONLY)
unset(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE)

function(asio_grpc_add_test _asio-grpc_name _asio_grpc_type)
    add_executable(${_asio-grpc_name})

    target_sources(
        ${_asio-grpc_name}
        PRIVATE # cmake-format: sort
                "${ASIO_GRPC_GENERATED_SOURCES}"
                "${CMAKE_CURRENT_BINARY_DIR}/generated/utils/memoryResource.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/asioUtils.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcClientServerTest.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcClientServerTest.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcContextTest.hpp"
                ${ARGN})

    if(ASIO_GRPC_TEST_COVERAGE)
        asio_grpc_add_coverage_flags(${_asio-grpc_name})
    endif()

    target_link_libraries(${_asio-grpc_name} PRIVATE asio-grpc-common-compile-options doctest::doctest Boost::coroutine
                                                     Boost::thread)
    if(${_asio_grpc_type} STREQUAL "STANDALONE_ASIO")
        target_link_libraries(${_asio-grpc_name} PRIVATE asio-grpc-standalone-asio asio::asio)
    elseif(${_asio_grpc_type} STREQUAL "BOOST_ASIO")
        target_link_libraries(${_asio-grpc_name} PRIVATE asio-grpc Boost::headers)
    elseif(${_asio_grpc_type} STREQUAL "UNIFEX")
        target_link_libraries(${_asio-grpc_name} PRIVATE asio-grpc-unifex unofficial::unifex)
    endif()

    target_include_directories(
        ${_asio-grpc_name}
        PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
               "$<BUILD_INTERFACE:${ASIO_GRPC_GENERATED_PROTOS_INCLUDE_DIR}>"
               "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>" $<INSTALL_INTERFACE:include>)

    target_precompile_headers(${_asio-grpc_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utils/precompiledHeader.hpp")
endfunction()

set(ASIO_GRPC_TEST_SOURCE_FILES "test-asio-grpc.cpp")

asio_grpc_add_test(asio-grpc-test-boost-cpp17 "BOOST_ASIO" ${ASIO_GRPC_TEST_SOURCE_FILES})
target_compile_definitions(asio-grpc-test-boost-cpp17 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"Boost.Asio C++17\"")

asio_grpc_add_test(asio-grpc-test-cpp17 "STANDALONE_ASIO" ${ASIO_GRPC_TEST_SOURCE_FILES})
target_compile_definitions(asio-grpc-test-cpp17 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"Standalone Asio C++17\""
                                                        AGRPC_STANDALONE_ASIO)

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_test(asio-grpc-test-boost-cpp20 "BOOST_ASIO" ${ASIO_GRPC_TEST_SOURCE_FILES} "test-asio-grpc-20.cpp")
    target_compile_definitions(asio-grpc-test-boost-cpp20 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"Boost.Asio C++20\"")
    target_link_libraries(asio-grpc-test-boost-cpp20 PRIVATE asio-grpc-cpp20-compile-options)

    asio_grpc_add_test(asio-grpc-test-cpp20 "STANDALONE_ASIO" ${ASIO_GRPC_TEST_SOURCE_FILES} "test-asio-grpc-20.cpp")
    target_compile_definitions(asio-grpc-test-cpp20 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"Standalone Asio C++20\""
                                                            AGRPC_STANDALONE_ASIO)
    target_link_libraries(asio-grpc-test-cpp20 PRIVATE asio-grpc-cpp20-compile-options)

    asio_grpc_add_test(asio-grpc-test-unifex "UNIFEX" "test-asio-grpc-unifex.cpp")
    target_compile_definitions(asio-grpc-test-unifex PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"unifex C++20\"")
    target_link_libraries(asio-grpc-test-unifex PRIVATE asio-grpc-cpp20-compile-options)
endif()

unset(ASIO_GRPC_TEST_SOURCE_FILES)

# tests for examples
add_executable(asio-grpc-test-examples)

target_sources(
    asio-grpc-test-examples
    PRIVATE # cmake-format: sort
            "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/test-asio-grpc-examples.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.hpp")

target_include_directories(asio-grpc-test-examples PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                           $<INSTALL_INTERFACE:include>)

target_link_libraries(asio-grpc-test-examples PRIVATE asio-grpc-common-compile-options doctest::doctest Boost::headers
                                                      Boost::filesystem)

add_dependencies(asio-grpc-test-examples asio-grpc-example-streaming-client-cpp20
                 asio-grpc-example-streaming-server-cpp20)

target_compile_definitions(
    asio-grpc-test-examples
    PRIVATE "ASIO_GRPC_EXAMPLE_STREAMING_CLIENT=\"$<TARGET_FILE:asio-grpc-example-streaming-client-cpp20>\""
            "ASIO_GRPC_EXAMPLE_STREAMING_SERVER=\"$<TARGET_FILE:asio-grpc-example-streaming-server-cpp20>\"")

if(ASIO_GRPC_DISCOVER_TESTS)
    doctest_discover_tests(
        asio-grpc-test-boost-cpp17 # workaround for regression in doctest 2.4.6
        #
        # if given arguments:
        #
        # "EQUAL" "1"
        #
        # Unknown arguments specified
        ADD_LABELS 0)
    doctest_discover_tests(asio-grpc-test-cpp17 ADD_LABELS 0)
    doctest_discover_tests(asio-grpc-test-examples ADD_LABELS 0)
    if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
        doctest_discover_tests(asio-grpc-test-boost-cpp20 ADD_LABELS 0)
        doctest_discover_tests(asio-grpc-test-cpp20 ADD_LABELS 0)
        doctest_discover_tests(asio-grpc-test-unifex ADD_LABELS 0)
    endif()
endif()

if(ASIO_GRPC_TEST_COVERAGE)
    if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
        asio_grpc_coverage_report_for_target(asio-grpc-test-cpp20)
    else()
        asio_grpc_coverage_report_for_target(asio-grpc-test-cpp17)
    endif()
endif()

# cmake tests
add_test(
    NAME asio-grpc-cmake-protobuf-generate
    COMMAND
        "${CMAKE_COMMAND}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DCMAKE_GENERATOR=${CMAKE_GENERATOR}"
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
        "-DPWD=${CMAKE_CURRENT_BINARY_DIR}/protobuf-generate" "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}" "-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/cmake"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}" "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
        "-DVCPKG_OVERLAY_PORTS=${VCPKG_OVERLAY_PORTS}" "-DVCPKG_MANIFEST_MODE=${VCPKG_MANIFEST_MODE}"
        "-DBoost_USE_STATIC_RUNTIME=${ASIO_GRPC_BOOST_USE_STATIC_RUNTIME}"
        "-DCMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DASIO_GRPC_USE_BOOST_CONTAINER=${ASIO_GRPC_USE_BOOST_CONTAINER}" -P
        "${CMAKE_CURRENT_LIST_DIR}/cmake/Test.cmake")
