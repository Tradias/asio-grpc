# test protobuf sources
asio_grpc_protobuf_generate(
    GENERATE_GRPC GENERATE_MOCK_CODE
    OUT_VAR "ASIO_GRPC_TEXT_PROTO_SOURCES"
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/test/v1/test.proto"
           "${CMAKE_CURRENT_SOURCE_DIR}/proto/test/msg/message.proto")

function(asio_grpc_add_test_proto_target _asio_grpc_name _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(${_asio_grpc_name} PRIVATE ${ASIO_GRPC_TEXT_PROTO_SOURCES})

    target_link_libraries(${_asio_grpc_name} PUBLIC protobuf::libprotobuf)

    target_include_directories(${_asio_grpc_name} SYSTEM
                               PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(${_asio_grpc_name} PRIVATE asio-grpc-compile-options${_asio_grpc_cxx_standard}
                                                     asio-grpc-coverage-options)
endfunction()

asio_grpc_add_test_proto_target(asio-grpc-test-protobuf "17")
asio_grpc_add_test_proto_target(asio-grpc-test-protobuf-cpp20 "20")

# test main
function(asio_grpc_add_test_main _asio_grpc_name _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(
        ${_asio_grpc_name}
        PRIVATE # cmake-format: sort
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/client_context.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/client_context.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/doctest.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/free_port.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/free_port.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcFormat.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpcFormat.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/main.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/time.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/time.hpp")

    target_include_directories(${_asio_grpc_name} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(
        ${_asio_grpc_name}
        PUBLIC doctest::doctest GTest::gmock asio-grpc-compile-options${_asio_grpc_cxx_standard}
               asio-grpc-test-protobuf${_asio_grpc_cxx_standard} asio-grpc-coverage-options
        PRIVATE Boost::headers Boost::filesystem)
endfunction()

asio_grpc_add_test_main(asio-grpc-test-main "17")
asio_grpc_add_test_main(asio-grpc-test-main-cpp20 "20")

# test utils
if(ASIO_GRPC_USE_BOOST_CONTAINER)
    set(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE [[#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/global_resource.hpp>]])
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/utils/memory_resource.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/utils/memory_resource.hpp" @ONLY)
unset(ASIO_GRPC_MEMORY_RESOURCE_UTILS_INCLUDE)

function(asio_grpc_add_test_util _asio_grpc_name _asio_grpc_type _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(
        ${_asio_grpc_name}
        PRIVATE # cmake-format: sort
                "${CMAKE_CURRENT_BINARY_DIR}/generated/utils/memory_resource.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/asio_separate_compilation.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/asio_utils.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_client_server_test.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_client_server_test.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_client_server_test_base.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_client_server_test_base.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_context_test.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_context_test.hpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_generic_client_server_test.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/utils/grpc_generic_client_server_test.hpp")
    if(NOT ${_asio_grpc_type} STREQUAL "UNIFEX")
        target_sources(
            ${_asio_grpc_name}
            PRIVATE # cmake-format: sort
                    "${CMAKE_CURRENT_SOURCE_DIR}/utils/rpc.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/utils/rpc.hpp")
    endif()

    target_include_directories(${_asio_grpc_name} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                         "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")

    target_precompile_headers(${_asio_grpc_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/utils/precompiled_header.hpp")

    if(${_asio_grpc_type} STREQUAL "BOOST_ASIO")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc)

        target_compile_definitions(
            ${_asio_grpc_name} PUBLIC "ASIO_GRPC_TEST_CPP_VERSION=\"Boost.Asio C++${_asio_grpc_cxx_standard}\""
                                      BOOST_ASIO_SEPARATE_COMPILATION BOOST_ASIO_NO_TS_EXECUTORS)
    elseif(${_asio_grpc_type} STREQUAL "STANDALONE_ASIO")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-standalone-asio)

        target_compile_definitions(
            ${_asio_grpc_name} PUBLIC "ASIO_GRPC_TEST_CPP_VERSION=\"Standalone Asio C++${_asio_grpc_cxx_standard}\""
                                      ASIO_SEPARATE_COMPILATION ASIO_NO_TS_EXECUTORS)
    elseif(${_asio_grpc_type} STREQUAL "UNIFEX")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-unifex)

        target_compile_definitions(${_asio_grpc_name}
                                   PUBLIC "ASIO_GRPC_TEST_CPP_VERSION=\"unifex C++${_asio_grpc_cxx_standard}\"")
    endif()

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(
        ${_asio_grpc_name} PUBLIC asio-grpc-test-main${_asio_grpc_cxx_standard}
                                  asio-grpc-test-protobuf${_asio_grpc_cxx_standard} asio-grpc-coverage-options)
endfunction()

asio_grpc_add_test_util(asio-grpc-test-util-boost-asio "BOOST_ASIO" "17")
asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio "STANDALONE_ASIO" "17")

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_test_util(asio-grpc-test-util-boost-asio-cpp20 "BOOST_ASIO" "20")
    asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio-cpp20 "STANDALONE_ASIO" "20")
    asio_grpc_add_test_util(asio-grpc-test-util-unifex-cpp20 "UNIFEX" "20")
endif()
