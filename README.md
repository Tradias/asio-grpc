# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc)

This library provides an implementation of [boost::asio::execution_context](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/execution_context.html) that dispatches work to a [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html). Making it possible to write 
asynchronous gRPC servers and clients using C++20 coroutines, Boost.Coroutines, Boost.Asio's stackless coroutines, std::futures and callbacks. Also enables other Boost.Asio non-blocking IO operations like HTTP requests - all on the same CompletionQueue.

# Requirements

Tested:

 * gRPC 1.37
 * Boost 1.74
 * MSVC VS 2019 16.11
 * GCC 10.3
 * C++17 or C++20

For MSVC compilers the following compile definitions might need to be set:

```
BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
```

# Usage

The library can be added to a CMake project using either `add_subdirectory` or `find_package` . Once set up, include the following header:

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

# Performance

asio-grpc is part of [grpc_bench](https://github.com/LesnyRumcajs/grpc_bench). Head over there to compare its performance against other libraries and languages.

Results from the helloworld unary RPC. Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, Boost 1.74, gRPC 1.30.2, asio-grpc v1.0.0

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_tonic_mt               |   44639 |       22.27 ms |        9.63 ms |       10.55 ms |      572.53 ms |  101.12% |     16.06 MiB |
| rust_grpcio                 |   39826 |       24.95 ms |       26.31 ms |       27.19 ms |       28.45 ms |   101.5% |     30.46 MiB |
| rust_thruster_mt            |   38038 |       26.17 ms |       11.39 ms |       12.33 ms |      673.02 ms |  100.16% |     13.17 MiB |
| cpp_grpc_mt                 |   34954 |       28.53 ms |       31.28 ms |       31.75 ms |       33.55 ms |  101.93% |      8.36 MiB |
| cpp_asio_grpc               |   34015 |       29.32 ms |       32.05 ms |       32.56 ms |       34.41 ms |  101.35% |      7.72 MiB |
| go_grpc                     |    6772 |      141.75 ms |      287.57 ms |      330.45 ms |      499.47 ms |    97.8% |     28.07 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_tonic_mt               |   66253 |       14.33 ms |       39.24 ms |       59.11 ms |       91.03 ms |   201.2% |     16.09 MiB |
| rust_grpcio                 |   62678 |       15.38 ms |       22.38 ms |       24.81 ms |       29.00 ms |  201.38% |     45.07 MiB |
| cpp_grpc_mt                 |   62488 |       14.78 ms |       31.76 ms |       40.60 ms |       60.79 ms |  199.84% |      24.9 MiB |
| cpp_asio_grpc               |   62040 |       14.91 ms |       30.17 ms |       37.77 ms |       60.10 ms |   199.6% |     26.65 MiB |
| rust_thruster_mt            |   59204 |       16.22 ms |       43.04 ms |       71.87 ms |      110.07 ms |  199.31% |     13.87 MiB |
| go_grpc                     |   13978 |       63.48 ms |      110.86 ms |      160.62 ms |      205.85 ms |  198.23% |     29.48 MiB |

# Documentation

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [boost::asio::execution_context](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Boost.Asio functions that expect an `ExecutionContext` like [boost::asio::spawn](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/spawn/overload7.html).

Likewise, the `agrpc::GrpcExecutor` models the [Executor and Networking TS requirements](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and can therefore be used in places where Boost.Asio expects an `Executor`.

## Getting started

Start by creating a `agrpc::GrpcContext`.

For servers and clients:

<!-- snippet: create-grpc_context-server-side -->
<a id='snippet-create-grpc_context-server-side'></a>
```cpp
grpc::ServerBuilder builder;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
```
<sup><a href='/example/example-server.cpp#L124-L127' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/example/example-client.cpp#L142-L144' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` (shown further below) and run it. Make sure to shutdown the `server` before destructing the `grpc_context`. Also destruct the `grpc_context` before destructing the `server`. A `grpc_context` can only be run on one thread at a time.

<!-- snippet: run-grpc_context-server-side -->
<a id='snippet-run-grpc_context-server-side'></a>
```cpp
grpc_context.run();
server->Shutdown();
}  // grpc_context is destructed here before the server
```
<sup><a href='/example/example-server.cpp#L140-L144' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
auto guard = boost::asio::make_work_guard(grpc_context);
```
<sup><a href='/example/example-client.cpp#L146-L148' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Alarm

gRPC provides a [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html) which similar to [boost::asio::steady_timer](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/steady_timer.html). Simply construct it and pass to it `agrpc::wait` with the desired deadline to wait for the specified amount of time without blocking the event loop.

<!-- snippet: alarm -->
<a id='snippet-alarm'></a>
```cpp
grpc::Alarm alarm;
bool wait_ok = agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(1), yield);
```
<sup><a href='/example/example-server.cpp#L26-L29' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

`wait_ok` is true if the Alarm expired, false if it was canceled. ([source](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a))

## Unary RPC Server-Side

Start by requesting a RPC. In this example `yield` is a [boost::asio::yield_context](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/yield_context.html), other [CompletionToken](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers)s are supported as well, e.g. [boost::asio::use_awaitable](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/use_awaitable.html). The `example` namespace has been generated from [example.proto](/example/protos/example.proto).

<!-- snippet: request-unary-server-side -->
<a id='snippet-request-unary-server-side'></a>
```cpp
grpc::ServerContext server_context;
example::v1::Request request;
grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service, server_context,
                                 request, writer, yield);
```
<sup><a href='/example/example-server.cpp#L34-L40' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

If `request_ok` is true then the RPC has indeed been started otherwise the server has been shutdown before this particular request got matched to an incoming RPC. For a full list of ok-values returned by gRPC see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

The `grpc::ServerAsyncResponseWriter` is used to drive the RPC. The following actions can be performed.

<!-- snippet: unary-server-side -->
<a id='snippet-unary-server-side'></a>
```cpp
bool send_ok = agrpc::send_initial_metadata(writer, yield);

example::v1::Response response;
bool finish_ok = agrpc::finish(writer, response, grpc::Status::OK, yield);

bool finish_with_error_ok = agrpc::finish_with_error(writer, grpc::Status::CANCELLED, yield);
```
<sup><a href='/example/example-server.cpp#L42-L49' title='Snippet source file'>snippet source</a> | <a href='#snippet-unary-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Unary RPC Client-Side

On the client-side a RPC is initiated by calling the desired `AsyncXXX` function of the `Stub`

<!-- snippet: request-unary-client-side -->
<a id='snippet-request-unary-client-side'></a>
```cpp
grpc::ClientContext client_context;
example::v1::Request request;
std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
    stub.AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
```
<sup><a href='/example/example-client.cpp#L24-L29' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

The `grpc::ClientAsyncResponseReader` is used to drive the RPC.

<!-- snippet: unary-client-side -->
<a id='snippet-unary-client-side'></a>
```cpp
bool read_ok = agrpc::read_initial_metadata(*reader, yield);

example::v1::Response response;
grpc::Status status;
bool finish_ok = agrpc::finish(*reader, response, status, yield);
```
<sup><a href='/example/example-client.cpp#L30-L36' title='Snippet source file'>snippet source</a> | <a href='#snippet-unary-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## Client-Streaming RPC Server-Side

Start by requesting a RPC.

<!-- snippet: request-client-streaming-server-side -->
<a id='snippet-request-client-streaming-server-side'></a>
```cpp
grpc::ServerContext server_context;
grpc::ServerAsyncReader<example::v1::Response, example::v1::Request> reader{&server_context};
bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
                                 server_context, reader, yield);
```
<sup><a href='/example/example-server.cpp#L54-L59' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Drive the RPC with the following functions.

<!-- snippet: client-streaming-server-side -->
<a id='snippet-client-streaming-server-side'></a>
```cpp
bool send_ok = agrpc::send_initial_metadata(reader, yield);

example::v1::Request request;
bool read_ok = agrpc::read(reader, request, yield);

example::v1::Response response;
bool finish_ok = agrpc::finish(reader, response, grpc::Status::OK, yield);
```
<sup><a href='/example/example-server.cpp#L61-L69' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-streaming-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Client-Streaming RPC Client-Side

Start by requesting a RPC.

<!-- snippet: request-client-streaming-client-side -->
<a id='snippet-request-client-streaming-client-side'></a>
```cpp
grpc::ClientContext client_context;
example::v1::Response response;
std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;
bool request_ok = agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context, writer,
                                 response, yield);
```
<sup><a href='/example/example-client.cpp#L51-L57' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncWriter` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-client-streaming-client-side-alt -->
<a id='snippet-request-client-streaming-client-side-alt'></a>
```cpp
auto [writer, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context, response, yield);
```
<sup><a href='/example/example-client.cpp#L43-L46' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-client-side-alt' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

With the `grpc::ClientAsyncWriter` the following actions can be performed to drive the RPC.

<!-- snippet: client-streaming-client-side -->
<a id='snippet-client-streaming-client-side'></a>
```cpp
bool read_ok = agrpc::read_initial_metadata(*writer, yield);

example::v1::Request request;
bool write_ok = agrpc::write(*writer, request, yield);

bool writes_done_ok = agrpc::writes_done(*writer, yield);

grpc::Status status;
bool finish_ok = agrpc::finish(*writer, status, yield);
```
<sup><a href='/example/example-client.cpp#L59-L69' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_ok`, `write_ok`, `writes_done_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## Server-Streaming RPC Server-Side

Start by requesting a RPC.

<!-- snippet: request-server-streaming-server-side -->
<a id='snippet-request-server-streaming-server-side'></a>
```cpp
grpc::ServerContext server_context;
example::v1::Request request;
grpc::ServerAsyncWriter<example::v1::Response> writer{&server_context};
bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestServerStreaming, service,
                                 server_context, request, writer, yield);
```
<sup><a href='/example/example-server.cpp#L74-L80' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

With the `grpc::ServerAsyncWriter` the following actions can be performed to drive the RPC.

<!-- snippet: server-streaming-server-side -->
<a id='snippet-server-streaming-server-side'></a>
```cpp
bool send_ok = agrpc::send_initial_metadata(writer, yield);

example::v1::Response response;
bool write_ok = agrpc::write(writer, response, yield);

bool write_and_finish_ok = agrpc::write_and_finish(writer, response, grpc::WriteOptions{}, grpc::Status::OK, yield);

bool finish_ok = agrpc::finish(writer, grpc::Status::OK, yield);
```
<sup><a href='/example/example-server.cpp#L82-L91' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-streaming-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `send_ok`, `write_ok`, `write_and_finish` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## Server-Streaming RPC Client-Side

Start by requesting a RPC.

<!-- snippet: request-server-streaming-client-side -->
<a id='snippet-request-server-streaming-client-side'></a>
```cpp
grpc::ClientContext client_context;
example::v1::Request request;
std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
bool request_ok =
    agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, reader, yield);
```
<sup><a href='/example/example-client.cpp#L84-L90' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncReader` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-server-streaming-client-side-alt -->
<a id='snippet-request-server-streaming-client-side-alt'></a>
```cpp
auto [reader, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, yield);
```
<sup><a href='/example/example-client.cpp#L76-L79' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-client-side-alt' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

With the `grpc::ClientAsyncReader` the following actions can be performed to drive the RPC.

<!-- snippet: server-streaming-client-side -->
<a id='snippet-server-streaming-client-side'></a>
```cpp
bool read_metadata_ok = agrpc::read_initial_metadata(*reader, yield);

example::v1::Response response;
bool read_ok = agrpc::read(*reader, response, yield);

grpc::Status status;
bool finish_ok = agrpc::finish(*reader, status, yield);
```
<sup><a href='/example/example-client.cpp#L92-L100' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_metadata_ok`, `read_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## Bidirectional-Streaming RPC Server-Side

Start by requesting a RPC.

<!-- snippet: request-bidirectional-streaming-server-side -->
<a id='snippet-request-bidirectional-streaming-server-side'></a>
```cpp
grpc::ServerContext server_context;
grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request> reader_writer{&server_context};
bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestBidirectionalStreaming, service,
                                 server_context, reader_writer, yield);
```
<sup><a href='/example/example-server.cpp#L96-L101' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-streaming-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

With the `grpc::ServerAsyncReaderWriter` the following actions can be performed to drive the RPC.

<!-- snippet: bidirectional-streaming-server-side -->
<a id='snippet-bidirectional-streaming-server-side'></a>
```cpp
bool send_ok = agrpc::send_initial_metadata(reader_writer, yield);

example::v1::Request request;
bool read_ok = agrpc::read(reader_writer, request, yield);

example::v1::Response response;
bool write_and_finish_ok =
    agrpc::write_and_finish(reader_writer, response, grpc::WriteOptions{}, grpc::Status::OK, yield);

bool write_ok = agrpc::write(reader_writer, response, yield);

bool finish_ok = agrpc::finish(reader_writer, grpc::Status::OK, yield);
```
<sup><a href='/example/example-server.cpp#L103-L116' title='Snippet source file'>snippet source</a> | <a href='#snippet-bidirectional-streaming-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `send_ok`, `read_ok`, `write_and_finish_ok`, `write_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## Bidirectional-Streaming RPC Client-Side

Start by requesting a RPC.

<!-- snippet: request-bidirectional-client-side -->
<a id='snippet-request-bidirectional-client-side'></a>
```cpp
grpc::ClientContext client_context;
std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;
bool request_ok = agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub, client_context,
                                 reader_writer, yield);
```
<sup><a href='/example/example-client.cpp#L114-L119' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncReaderWriter` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-bidirectional-client-side-alt -->
<a id='snippet-request-bidirectional-client-side-alt'></a>
```cpp
auto [reader_writer, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub, client_context, yield);
```
<sup><a href='/example/example-client.cpp#L106-L109' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-client-side-alt' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

With the `grpc::ClientAsyncReaderWriter` the following actions can be performed to drive the RPC.

<!-- snippet: bidirectional-client-side -->
<a id='snippet-bidirectional-client-side'></a>
```cpp
bool read_metadata_ok = agrpc::read_initial_metadata(*reader_writer, yield);

example::v1::Request request;
bool write_ok = agrpc::write(*reader_writer, request, yield);

bool writes_done_ok = agrpc::writes_done(*reader_writer, yield);

example::v1::Response response;
bool read_ok = agrpc::read(*reader_writer, response, yield);

grpc::Status status;
bool finish_ok = agrpc::finish(*reader_writer, status, yield);
```
<sup><a href='/example/example-client.cpp#L121-L134' title='Snippet source file'>snippet source</a> | <a href='#snippet-bidirectional-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_metadata_ok`, `write_ok`, `writes_done_ok`, `read_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).
