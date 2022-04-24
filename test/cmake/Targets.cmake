# common compile options
add_library(compile-options INTERFACE)

target_link_libraries(compile-options INTERFACE Boost::coroutine Boost::thread Boost::disable_autolinking)

target_compile_definitions(compile-options INTERFACE $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0A00> # Windows 10
                                                     BOOST_ASIO_NO_DEPRECATED ASIO_NO_DEPRECATED)

function(create_object_library _name)
    add_library(${_name} OBJECT)

    target_sources(${_name} PRIVATE ${ARGN})

    target_link_libraries(${_name} PRIVATE compile-options)
endfunction()

# TARGET option
create_object_library(target-option target.cpp)

target_link_libraries(target-option PRIVATE asio-grpc::asio-grpc-standalone-asio)

asio_grpc_protobuf_generate(
    GENERATE_GRPC GENERATE_MOCK_CODE
    TARGET target-option
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/target"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/target.proto")

# OUT_VAR option
set(OUT_VAR_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/outVar/")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    OUT_VAR OUT_VAR_GENERATED_SOURCES
    OUT_DIR "${OUT_VAR_GENERATED_PROTOS_OUT_DIR}"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/out_var/v1/outVar.proto"
           "${CMAKE_CURRENT_SOURCE_DIR}/proto/out_var/msg/message.proto"
           "${CMAKE_CURRENT_SOURCE_DIR}/proto/out_var/subdir/other.proto")

create_object_library(out-var-option outVar.cpp ${OUT_VAR_GENERATED_SOURCES})

target_link_libraries(out-var-option PRIVATE asio-grpc::asio-grpc)

target_include_directories(out-var-option PRIVATE "${OUT_VAR_GENERATED_PROTOS_OUT_DIR}")

# DESCRIPTOR option
create_object_library(descriptor-option descriptor.cpp)

target_link_libraries(descriptor-option PRIVATE asio-grpc::asio-grpc)

set(DESCRIPTOR_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/descriptor")

asio_grpc_protobuf_generate(
    GENERATE_DESCRIPTORS
    TARGET descriptor-option
    OUT_DIR "${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/descriptor.proto")

target_include_directories(descriptor-option PRIVATE "${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}")

target_compile_definitions(descriptor-option
                           PRIVATE "DESCRIPTOR_FILE=R\"(${DESCRIPTOR_GENERATED_PROTOS_OUT_DIR}/descriptor.desc)\"")

# main
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE target-option out-var-option descriptor-option compile-options)
