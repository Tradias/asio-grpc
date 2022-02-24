# asio-grpc

## Overview

Feature overview, installation, performance benchmark and getting started instructions can be found on [github](https://github.com/Tradias/asio-grpc).

* Looking for the main workhorses of this library?
    * `agrpc::GrpcContext` and `agrpc::GrpcExecutor`.
* Want to run RPCs asynchronously?
    * `agrpc::finish`, `agrpc::finish_with_error`, `agrpc::read`, `agrpc::read_initial_metadata`, `agrpc::request`, `agrpc::repeatedly_request`, `agrpc::send_initial_metadata`, `agrpc::write`, `agrpc::write_and_finish`, `agrpc::writes_done`
* Looking to wait for a `grpc::Alarm`?
    * `agrpc::wait`
* Want to customize asynchronous completion?
    * [Completion tokens](md_doc_completion_tokens.html)
* Want to run `protoc` from CMake to generate gRPC source files?
    * [CMake protobuf generate](md_doc_cmake_protobuf_generate.html)
