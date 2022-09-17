# CMake protobuf generate

@snippet{doc} AsioGrpcProtobufGenerator.cmake asio_grpc_protobuf_generate

### Example

Given a CMake target called `target-option`:

@snippet test/cmake/superbuild/src/CMakeLists.txt asio_grpc_protobuf_generate-example

Compiling `target-option` will cause the generation and compilation of:

* `${CMAKE_CURRENT_BINARY_DIR}/target/target.pb.h`
* `${CMAKE_CURRENT_BINARY_DIR}/target/target.pb.cc`
* `${CMAKE_CURRENT_BINARY_DIR}/target/target.grpc.pb.h`
* `${CMAKE_CURRENT_BINARY_DIR}/target/target.grpc.pb.cc`
* `${CMAKE_CURRENT_BINARY_DIR}/target/target_mock.grpc.pb.h`

whenever `${CMAKE_CURRENT_SOURCE_DIR}/proto/target.proto` has been modified.
