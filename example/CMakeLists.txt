include(AsioGrpcProtobufGenerator)
set(ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR
    ASIO_GRPC_GENERATED_EXAMPLE_SOURCES
    PROTOC_OUT_DIR
    "${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR}"
    PROTOS
    "${CMAKE_CURRENT_LIST_DIR}/protos/helloworld.proto"
    "${CMAKE_CURRENT_LIST_DIR}/protos/example.proto")

function(asio_grpc_add_example _asio_grpc_name)
    add_executable(asio-grpc-${_asio_grpc_name})

    target_sources(asio-grpc-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp ${ASIO_GRPC_GENERATED_EXAMPLE_SOURCES})

    target_link_libraries(asio-grpc-${_asio_grpc_name} PRIVATE asio-grpc-common-compile-options)

    target_include_directories(asio-grpc-${_asio_grpc_name} PRIVATE ${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR})
endfunction()

asio_grpc_add_example(example-server)
target_link_libraries(asio-grpc-example-server PRIVATE Boost::coroutine Boost::thread)

asio_grpc_add_example(example-client)
target_link_libraries(asio-grpc-example-client PRIVATE Boost::coroutine Boost::thread)

asio_grpc_add_example(hello-world-server-cpp20)
target_link_libraries(asio-grpc-hello-world-server-cpp20 PRIVATE asio-grpc-cpp20-compile-options)

asio_grpc_add_example(hello-world-client-cpp20)
target_link_libraries(asio-grpc-hello-world-client-cpp20 PRIVATE asio-grpc-cpp20-compile-options)
