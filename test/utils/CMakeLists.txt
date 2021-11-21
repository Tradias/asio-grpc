# test main
include(AsioGrpcProtobufGenerator)

set(ASIO_GRPC_GENERATED_PROTOS_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(ASIO_GRPC_GENERATED_PROTOS_OUT_DIR "${ASIO_GRPC_GENERATED_PROTOS_INCLUDE_DIR}/protos")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR ASIO_GRPC_GENERATED_SOURCES
    OUT_DIR "${ASIO_GRPC_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/protos/test.proto")

function(asio_grpc_add_test_main _asio_grpc_name)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(
        ${_asio_grpc_name}
        PRIVATE # cmake-format: sort
                "${ASIO_GRPC_GENERATED_SOURCES}" "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/freePort.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/utils/main.cpp")

    target_include_directories(
        ${_asio_grpc_name}
        PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
               "$<BUILD_INTERFACE:${ASIO_GRPC_GENERATED_PROTOS_INCLUDE_DIR}>" $<INSTALL_INTERFACE:include>)

    target_link_libraries(
        ${_asio_grpc_name}
        PUBLIC doctest::doctest asio-grpc-common-compile-options asio-grpc-coverage-options
        PRIVATE Boost::headers)
endfunction()

asio_grpc_add_test_main(asio-grpc-test-main)

asio_grpc_add_test_main(asio-grpc-test-main-cpp20)
target_link_libraries(asio-grpc-test-main-cpp20 PUBLIC asio-grpc-cpp20-compile-options)

# test utils
if(ASIO_GRPC_USE_BOOST_CONTAINER)
    set(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE [[#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/global_resource.hpp>]])
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/utils/memoryResource.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/utils/memoryResource.hpp" @ONLY)
unset(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE)

function(asio_grpc_add_test_util _asio_grpc_name _asio_grpc_type _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(
        ${_asio_grpc_name}
        PRIVATE # cmake-format: sort
                "${CMAKE_CURRENT_BINARY_DIR}/generated/utils/memoryResource.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/asioSeparateCompilation.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/asioUtils.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcClientServerTest.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcClientServerTest.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcContextTest.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcContextTest.hpp")

    target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-common-compile-options asio-grpc-coverage-options)

    if(${_asio_grpc_type} STREQUAL "BOOST_ASIO")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc)

        target_compile_definitions(${_asio_grpc_name} PUBLIC BOOST_ASIO_SEPARATE_COMPILATION)
    elseif(${_asio_grpc_type} STREQUAL "STANDALONE_ASIO")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-standalone-asio)

        target_compile_definitions(${_asio_grpc_name} PUBLIC ASIO_SEPARATE_COMPILATION)
    elseif(${_asio_grpc_type} STREQUAL "UNIFEX")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-unifex)
    endif()

    target_include_directories(
        ${_asio_grpc_name}
        PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
               "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>" $<INSTALL_INTERFACE:include>)

    target_precompile_headers(${_asio_grpc_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utils/precompiledHeader.hpp")

    if(${_asio_grpc_cxx_standard} STREQUAL "20")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-cpp20-compile-options)
    endif()
endfunction()

asio_grpc_add_test_util(asio-grpc-test-util-boost-asio "BOOST_ASIO" "17")
asio_grpc_add_test_util(asio-grpc-test-util-boost-asio-cpp20 "BOOST_ASIO" "20")

asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio "STANDALONE_ASIO" "17")
asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio-cpp20 "STANDALONE_ASIO" "20")

asio_grpc_add_test_util(asio-grpc-test-util-unifex "UNIFEX" "17")
asio_grpc_add_test_util(asio-grpc-test-util-unifex-cpp20 "UNIFEX" "20")
