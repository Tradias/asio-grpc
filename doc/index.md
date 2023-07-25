# asio-grpc

## Overview

Feature overview, installation, performance benchmark and getting started instructions can be found on [github](https://github.com/Tradias/asio-grpc).

* Want to get an overview of the entire API?
    * [agrpc namespace](namespaceagrpc.html)
* Looking for the main workhorses of this library?
    * `agrpc::GrpcContext` and `agrpc::GrpcExecutor`.
* Want to run RPCs asynchronously?
    * [RPC cheat sheet](md_doc_rpc_cheat_sheet.html)
    * `agrpc::finish`, `agrpc::finish_with_error`, `agrpc::read`, `agrpc::read_initial_metadata`, `agrpc::request`, `agrpc::repeatedly_request`, `agrpc::send_initial_metadata`, `agrpc::write`, `agrpc::write_and_finish`, `agrpc::write_last`, `agrpc::writes_done`, `agrpc::notify_when_done`, `agrpc::notify_on_state_change`
* Looking for a convenient way to implement asynchronous gRPC clients?
    * `agrpc::ClientRPC`
* Looking to wait for a `grpc::Alarm`?
    * `agrpc::Alarm`, `agrpc::wait`
* Already using an `asio::io_context`?
    * `agrpc::run`, `agrpc::run_completion_queue` (experimental)
* Looking for a faster, drop-in replacement for gRPC's [DefaultHealthCheckService](https://github.com/grpc/grpc/blob/v1.50.1/src/cpp/server/health/default_health_check_service.h)?
    * `agrpc::HealthCheckService`
* Want to write Rust/Golang [select](https://go.dev/ref/spec#Select_statements)-style code?
    * `agrpc::GrpcStream` (experimental)
* Want to customize asynchronous completion?
    * [Completion token](md_doc_completion_token.html)
* Want to customize allocation?
    * `agrpc::bind_allocator`
* Want to run `protoc` from CMake to generate gRPC source files?
    * [CMake protobuf generate](md_doc_cmake_protobuf_generate.html)
