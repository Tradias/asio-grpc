# asio-grpc

This library provides an implementation of Boost.Asio's [execution_context](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/execution_context.html) that dispatches work to a gRPC [CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html). Making it possible to write 
asynchronous gRPC servers and clients using C++20 coroutines, Boost.Coroutines, Boost.Asio's stackless coroutines, std::futures and callbacks.

# Requirements

Tested:

 * gRPC 1.37
 * Boost 1.76
 * MSVC VS 2019 16.11
 * GCC 10.3

For MSVC compilers the following compile definitions might need to be set:

```
BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
```

# Usage

The library can be used through `add_subdirectory` or `find_package` in a CMake project. Once set up, include the following header:

```c++
#include <agrpc/asioGrpc.hpp>
```

## As a subdirectory

Clone the repository into a subdirectory of your CMake project. Then add it and link it to your target.

```cmake
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

## As a CMake package

Clone the repository and install it.

```shell
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/desired/installation/directory ..
cmake --build . --target install
```

Locate it and link it to your target.

```cmake
# Make sure to set CMAKE_PREFIX_PATH to /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

# Example

Server side:

<!-- snippet: server-side-helloworld -->
<a id='snippet-server-side-helloworld'></a>
```cpp
grpc::ServerBuilder builder;
std::unique_ptr<grpc::Server> server;
helloworld::Greeter::AsyncService service;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
builder.RegisterService(&service);
server = builder.BuildAndStart();

boost::asio::co_spawn(
    grpc_context,
    [&]() -> boost::asio::awaitable<void>
    {
        grpc::ServerContext server_context;
        helloworld::HelloRequest request;
        grpc::ServerAsyncResponseWriter<helloworld::HelloReply> writer{&server_context};
        bool request_ok = co_await agrpc::request(&helloworld::Greeter::AsyncService::RequestSayHello, service,
                                                  server_context, request, writer);
        helloworld::HelloReply response;
        std::string prefix("Hello ");
        response.set_message(prefix + request.name());
        bool finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
    },
    boost::asio::detached);

grpc_context.run();
server->Shutdown();
```
<sup><a href='/example/hello-world-server-cpp20.cpp#L25-L52' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Client side:

<!-- snippet: client-side-helloworld -->
<a id='snippet-client-side-helloworld'></a>
```cpp
auto stub =
    helloworld::Greeter::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

boost::asio::co_spawn(
    grpc_context,
    [&]() -> boost::asio::awaitable<void>
    {
        grpc::ClientContext client_context;
        helloworld::HelloRequest request;
        request.set_name("world");
        std::unique_ptr<grpc::ClientAsyncResponseReader<helloworld::HelloReply>> reader =
            stub->AsyncSayHello(&client_context, request, agrpc::get_completion_queue(grpc_context));
        helloworld::HelloReply response;
        grpc::Status status;
        bool ok = co_await agrpc::finish(*reader, response, status);
    },
    boost::asio::detached);

grpc_context.run();
```
<sup><a href='/example/hello-world-client-cpp20.cpp#L25-L46' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

# Documentation

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [asio::execution_context](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Boost.Asio functions that expect an `ExecutionContext` like [asio::spawn](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/spawn/overload7.html).

Likewise, the `agrpc::GrpcExecutor` models the Executor and Networking TS requirements described [here](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and can therefore be used in places where Boost.Asio expects an `Executor`.

## Getting started

Start by creating a `agrpc::GrpcContext`.

For servers and clients:

<!-- snippet: create-grpc_context-server-side -->
<a id='snippet-create-grpc_context-server-side'></a>
```cpp
grpc::ServerBuilder builder;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
```
<sup><a href='/example/hello-world-server.cpp#L43-L46' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/example/hello-world-client.cpp#L35-L37' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the context (shown further below) and run it. Make sure to shutdown the server before destructing the context.

<!-- snippet: run-grpc_context-server-side -->
<a id='snippet-run-grpc_context-server-side'></a>
```cpp
grpc_context.run();
server->Shutdown();
}  // grpc_context is destructed here
```
<sup><a href='/example/hello-world-server.cpp#L71-L75' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
auto guard = boost::asio::make_work_guard(grpc_context);
```
<sup><a href='/example/hello-world-client.cpp#L39-L41' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Server Unary RPC

Start by requesting a RPC. In this example `yield` is a `boost::asio::yield_context` and the `helloworld` namespace has been generated from the [gRPC example proto](https://github.com/grpc/grpc/blob/master/examples/protos/helloworld.proto).

<!-- snippet: request-unary-server-side -->
<a id='snippet-request-unary-server-side'></a>
```cpp
grpc::ServerContext server_context;
helloworld::HelloRequest request;
grpc::ServerAsyncResponseWriter<helloworld::HelloReply> writer{&server_context};
bool request_ok = agrpc::request(&helloworld::Greeter::AsyncService::RequestSayHello,
                                 service, server_context, request, writer, yield);
```
<sup><a href='/example/hello-world-server.cpp#L56-L62' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

If `request_ok` is true then the RPC has indeed been started otherwise the server has been shutdown before this particular request got matched to an incoming RPC. For a full list of ok-values returned by gRPC see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

The `grpc::ServerAsyncResponseWriter` is used to drive the RPC. The following actions can be performed.

<!-- snippet: send_initial_metadata-server-side -->
<a id='snippet-send_initial_metadata-server-side'></a>
```cpp
bool send_ok = agrpc::send_initial_metadata(writer, yield);
```
<sup><a href='/example/hello-world-server.cpp#L25-L27' title='Snippet source file'>snippet source</a> | <a href='#snippet-send_initial_metadata-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: finish-server-side -->
<a id='snippet-finish-server-side'></a>
```cpp
helloworld::HelloReply response;
std::string prefix("Hello ");
response.set_message(prefix + request.name());
bool finish_ok = agrpc::finish(writer, response, grpc::Status::OK, yield);
```
<sup><a href='/example/hello-world-server.cpp#L63-L68' title='Snippet source file'>snippet source</a> | <a href='#snippet-finish-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: finish_with_error-server-side -->
<a id='snippet-finish_with_error-server-side'></a>
```cpp
bool finish_ok = agrpc::finish_with_error(writer, grpc::Status::CANCELLED, yield);
```
<sup><a href='/example/hello-world-server.cpp#L33-L35' title='Snippet source file'>snippet source</a> | <a href='#snippet-finish_with_error-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Client Unary RPC

On the client-side a RPC is initiated by calling the desired `AsyncXXX` function of the `Stub`

<!-- snippet: request-unary-client-side -->
<a id='snippet-request-unary-client-side'></a>
```cpp
grpc::ClientContext client_context;
helloworld::HelloRequest request;
request.set_name("world");
std::unique_ptr<grpc::ClientAsyncResponseReader<helloworld::HelloReply>> reader =
    stub->AsyncSayHello(&client_context, request, agrpc::get_completion_queue(grpc_context));
```
<sup><a href='/example/hello-world-client.cpp#L45-L51' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

The `grpc::ClientAsyncResponseReader` is used to drive the RPC.

<!-- snippet: read_initial_metadata-client-side -->
<a id='snippet-read_initial_metadata-client-side'></a>
```cpp
bool read_ok = agrpc::read_initial_metadata(reader, yield);
```
<sup><a href='/example/hello-world-client.cpp#L25-L27' title='Snippet source file'>snippet source</a> | <a href='#snippet-read_initial_metadata-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: finish-client-side -->
<a id='snippet-finish-client-side'></a>
```cpp
helloworld::HelloReply response;
grpc::Status status;
bool finish_ok = agrpc::finish(*reader, response, status, yield);
```
<sup><a href='/example/hello-world-client.cpp#L52-L56' title='Snippet source file'>snippet source</a> | <a href='#snippet-finish-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).
