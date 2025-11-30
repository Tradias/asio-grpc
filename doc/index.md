# asio-grpc

## Overview

Feature overview, installation and performance benchmark can be found on [github](https://github.com/Tradias/asio-grpc).

* View of the entire API: [agrpc namespace](namespaceagrpc.html)
* (New) Asio'nized gRPC callback API: @link agrpc::BasicServerUnaryReactor @endlink, @link agrpc::BasicServerReadReactor @endlink, @link agrpc::BasicServerWriteReactor @endlink, @link agrpc::BasicServerBidiReactor @endlink, @link agrpc::BasicClientUnaryReactor @endlink, @link agrpc::BasicClientWriteReactor @endlink, @link agrpc::BasicClientReadReactor @endlink, @link agrpc::BasicClientBidiReactor @endlink
* Main workhorses of this library: @link agrpc::GrpcContext @endlink, @link  agrpc::GrpcExecutor @endlink.
* Asynchronous gRPC clients: [cheat sheet](md_doc_2client__rpc__cheat__sheet.html), @link agrpc::ClientRPC @endlink, 
* Asynchronous gRPC servers: [cheat sheet](md_doc_2server__rpc__cheat__sheet.html), @link agrpc::ServerRPC @endlink, @link agrpc::register_awaitable_rpc_handler @endlink, 
@link agrpc::register_yield_rpc_handler @endlink, @link agrpc::register_sender_rpc_handler @endlink, @link agrpc::register_callback_rpc_handler @endlink, @link agrpc::register_coroutine_rpc_handler @endlink
* GRPC Timer: @link agrpc::Alarm @endlink
* Combining GrpcContext and asio::io_context: @link agrpc::run @endlink, @link agrpc::run_completion_queue @endlink
* Faster, drop-in replacement for gRPC's [DefaultHealthCheckService](https://github.com/grpc/grpc/blob/v1.50.1/src/cpp/server/health/default_health_check_service.h): @link agrpc::HealthCheckService @endlink
* Writing Rust/Golang [select](https://go.dev/ref/spec#Select_statements)-style code: @link agrpc::Waiter @endlink
* Customizing asynchronous completion: [Completion token](md_doc_2completion__token.html)
* Running `protoc` from CMake to generate gRPC source files: [CMake protobuf generate](md_doc_2cmake__protobuf__generate.html)
