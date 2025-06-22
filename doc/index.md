# asio-grpc

## Overview

Feature overview, installation and performance benchmark can be found on [github](https://github.com/Tradias/asio-grpc).

* View of the entire API: [agrpc namespace](namespaceagrpc.html)
* (New) Asio'nized gRPC callback API: `agrpc::BasicServerUnaryReactor`, `agrpc::BasicServerReadReactor`, `agrpc::BasicServerWriteReactor`, `agrpc::BasicServerBidiReactor`, `agrpc::BasicClientUnaryReactor`, `agrpc::BasicClientWriteReactor`, `agrpc::BasicClientReadReactor`, `agrpc::BasicClientBidiReactor`
* Main workhorses of this library: `agrpc::GrpcContext`, `agrpc::GrpcExecutor`.
* Asynchronous gRPC clients: [cheat sheet](md_doc_2client__rpc__cheat__sheet.html), `agrpc::ClientRPC`, 
* Asynchronous gRPC servers: [cheat sheet](md_doc_2server__rpc__cheat__sheet.html), `agrpc::ServerRPC`, `agrpc::register_awaitable_rpc_handler`, 
`agrpc::register_yield_rpc_handler`, `agrpc::register_sender_rpc_handler`, `agrpc::register_callback_rpc_handler`, `agrpc::register_coroutine_rpc_handler`
* GRPC Timer: `agrpc::Alarm`
* Combining GrpcContext and `asio::io_context`: `agrpc::run`, `agrpc::run_completion_queue`
* Faster, drop-in replacement for gRPC's [DefaultHealthCheckService](https://github.com/grpc/grpc/blob/v1.50.1/src/cpp/server/health/default_health_check_service.h): `agrpc::HealthCheckService`
* Writing Rust/Golang [select](https://go.dev/ref/spec#Select_statements)-style code: `agrpc::Waiter`
* Customizing asynchronous completion: [Completion token](md_doc_2completion__token.html)
* Running `protoc` from CMake to generate gRPC source files: [CMake protobuf generate](md_doc_2cmake__protobuf__generate.html)
