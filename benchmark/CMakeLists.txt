include(AsioGrpcProtobufGenerator)
set(ASIO_GRPC_GENERATED_BENCHMARK_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR
    ASIO_GRPC_GENERATED_BENCHMARK_SOURCES
    PROTOC_OUT_DIR
    "${ASIO_GRPC_GENERATED_BENCHMARK_PROTOS_OUT_DIR}"
    PROTOS
    "${CMAKE_CURRENT_LIST_DIR}/protos/benchmark_service.proto"
    "${CMAKE_CURRENT_LIST_DIR}/protos/messages.proto")

function(asio_grpc_add_benchmark _asio_grpc_name)
    add_executable(asio-grpc-${_asio_grpc_name})

    target_sources(asio-grpc-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp ${ASIO_GRPC_GENERATED_BENCHMARK_SOURCES})

    target_link_libraries(asio-grpc-${_asio_grpc_name} PRIVATE asio-grpc::asio-grpc)

    target_include_directories(asio-grpc-${_asio_grpc_name} PRIVATE ${ASIO_GRPC_GENERATED_BENCHMARK_PROTOS_OUT_DIR}
                                                                    ${CMAKE_CURRENT_LIST_DIR})

    target_compile_options(
        asio-grpc-${_asio_grpc_name} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall
                                             -pedantic-errors>)

    target_compile_definitions(
        asio-grpc-${_asio_grpc_name}
        PRIVATE $<$<CXX_COMPILER_ID:MSVC>:
                BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
                BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
                BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
                BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
                _WIN32_WINNT=0x0A00 # Windows 10
                WINVER=0x0A00>
                BOOST_ASIO_NO_DEPRECATED)
endfunction()

asio_grpc_add_benchmark(benchmark-cpp20)
target_compile_features(asio-grpc-benchmark-cpp20 PRIVATE cxx_std_20)
target_compile_options(asio-grpc-benchmark-cpp20 PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fcoroutines>)
