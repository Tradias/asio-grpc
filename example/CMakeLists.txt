include(AsioGrpcProtobufGenerator)
set(ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR
    ASIO_GRPC_GENERATED_EXAMPLE_SOURCES
    PROTOC_OUT_DIR
    "${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR}"
    PROTOS
    "${CMAKE_CURRENT_LIST_DIR}/protos/helloworld.proto")

function(asio_grpc_add_example _asio_grpc_name)
    add_executable(asio-grpc-example-${_asio_grpc_name})

    target_sources(asio-grpc-example-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp
                                                                ${ASIO_GRPC_GENERATED_EXAMPLE_SOURCES})

    target_link_libraries(asio-grpc-example-${_asio_grpc_name} PRIVATE asio-grpc::asio-grpc)

    target_include_directories(asio-grpc-example-${_asio_grpc_name}
                               PRIVATE ${ASIO_GRPC_GENERATED_EXAMPLE_PROTOS_OUT_DIR})

    target_compile_definitions(
        asio-grpc-example-${_asio_grpc_name}
        PRIVATE $<$<CXX_COMPILER_ID:MSVC>:
                BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
                BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
                BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
                BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
                _WIN32_WINNT=0x0A00 # Windows 10
                WINVER=0x0A00>
                BOOST_ASIO_NO_DEPRECATED)

    add_test(NAME asio-grpc-example-${_asio_grpc_name} COMMAND asio-grpc-example-${_asio_grpc_name})
endfunction()

asio_grpc_add_example(hello-world-server)
target_link_libraries(asio-grpc-example-hello-world-server PRIVATE Boost::coroutine Boost::thread)

asio_grpc_add_example(hello-world-server-cpp20)
target_compile_features(asio-grpc-example-hello-world-server-cpp20 PRIVATE cxx_std_20)
target_compile_options(asio-grpc-example-hello-world-server-cpp20
                       PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fcoroutines>)
