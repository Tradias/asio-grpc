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

    target_link_libraries(asio-grpc-${_asio_grpc_name} PRIVATE asio-grpc-common-compile-options)

    target_include_directories(asio-grpc-${_asio_grpc_name} PRIVATE ${ASIO_GRPC_GENERATED_BENCHMARK_PROTOS_OUT_DIR}
                                                                    ${CMAKE_CURRENT_LIST_DIR})
endfunction()

asio_grpc_add_benchmark(benchmark-cpp20)
target_link_libraries(asio-grpc-benchmark-cpp20 PRIVATE asio-grpc-cpp20-compile-options)
