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

# test utils
add_library(asio-grpc-test-utils OBJECT)

target_sources(
    asio-grpc-test-utils
    PRIVATE "utils/asioUtils.hpp"
            "utils/grpcContextTest.hpp"
            "utils/grpcClientServerTest.cpp"
            "utils/grpcClientServerTest.hpp"
            "utils/freePort.cpp"
            "utils/freePort.hpp"
            "${ASIO_GRPC_GENERATED_SOURCES}")

target_compile_options(asio-grpc-test-utils PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4>
                                                    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic-errors>)

target_link_libraries(asio-grpc-test-utils PRIVATE asio-grpc)

target_include_directories(
    asio-grpc-test-utils PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                $<BUILD_INTERFACE:${ASIO_GRPC_GENERATED_PROTOS_OUT_DIR}> $<INSTALL_INTERFACE:include>)

# tests
if(ASIO_GRPC_TEST_COVERAGE)
    include(AsioGrpcCoverage)
endif()

function(asio_grpc_add_test _asio-grpc_name)
    add_executable(${_asio-grpc_name})

    target_sources(${_asio-grpc_name} PRIVATE main.cpp $<TARGET_PROPERTY:asio-grpc,ASIO_GRPC_HEADER_FILES> ${ARGN})

    if(ASIO_GRPC_TEST_COVERAGE)
        asio_grpc_add_coverage_flags(${_asio-grpc_name})
    endif()

    target_link_libraries(${_asio-grpc_name} PRIVATE asio-grpc-objects doctest::doctest asio-grpc-test-utils
                                                     Boost::coroutine Boost::thread)

    target_include_directories(${_asio-grpc_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                                                         $<INSTALL_INTERFACE:include>)

    target_precompile_headers(${_asio-grpc_name} PRIVATE "utils/precompiledHeader.hpp")
endfunction()

set(ASIO_GRPC_TEST_SOURCE_FILES "test-asio-grpc.cpp")

asio_grpc_add_test(asio-grpc-test-cpp17 ${ASIO_GRPC_TEST_SOURCE_FILES})
target_compile_definitions(asio-grpc-test-cpp17 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"C++17\"")

asio_grpc_add_test(asio-grpc-test-cpp20 ${ASIO_GRPC_TEST_SOURCE_FILES} "test-asio-grpc-20.cpp")
target_compile_definitions(asio-grpc-test-cpp20 PRIVATE "ASIO_GRPC_TEST_CPP_VERSION=\"C++20\"")
target_compile_features(asio-grpc-test-cpp20 PRIVATE cxx_std_20)
target_compile_options(asio-grpc-test-cpp20 PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fcoroutines>)

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
    asio_grpc_coverage_report_for_target(asio-grpc-test-cpp17 "${CMAKE_CURRENT_SOURCE_DIR}/test-asio-grpc.cpp")
endif()
