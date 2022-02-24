# CMake protobuf generate

In the same directory that called `find_package(asio-grpc)` a CMake function will be made available that can be used to generate gRPC source files from .proto schemas.

If you are using [cmake-format](https://github.com/cheshirekow/cmake_format) then you can copy the `asio_grpc_protobuf_generate` section from [cmake-format.yaml](https://github.com/Tradias/asio-grpc/blob/master/cmake-format.yaml#L2-L13) into your cmake-format.yaml to get proper formatting.

@snippet[doc] AsioGrpcProtobufGenerator.cmake asio_grpc_protobuf_generate