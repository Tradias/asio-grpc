# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc)

This library provides an implementation of [boost::asio::execution_context](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/execution_context.html) that dispatches work to a [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html). Making it possible to write 
asynchronous gRPC servers and clients using C++20 coroutines, Boost.Coroutines, Boost.Asio's stackless coroutines, std::futures and callbacks. Also enables other Boost.Asio non-blocking IO operations like HTTP requests - all on the same CompletionQueue.

# Example

Server side:

<!-- snippet: server-side-helloworld -->
<a id='snippet-server-side-helloworld'></a>
```cpp
grpc::ServerBuilder builder;
std::unique_ptr<grpc::Server> server;
helloworld::Greeter::AsyncService service;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
boost::asio::basic_signal_set signals{grpc_context, SIGINT, SIGTERM};
builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
builder.RegisterService(&service);
server = builder.BuildAndStart();

boost::asio::co_spawn(
    grpc_context,
    [&]() -> boost::asio::awaitable<void>
    {
        while (true)
        {
            grpc::ServerContext server_context;
            helloworld::HelloRequest request;
            grpc::ServerAsyncResponseWriter<helloworld::HelloReply> writer{&server_context};
            bool request_ok = co_await agrpc::request(&helloworld::Greeter::AsyncService::RequestSayHello, service,
                                                      server_context, request, writer);
            if (!request_ok)
            {
                co_return;
            }
            helloworld::HelloReply response;
            response.set_message("Hello " + request.name());
            bool finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
        }
    },
    boost::asio::detached);
```
<sup><a href='/example/hello-world-server-cpp20.cpp#L31-L62' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-helloworld' title='Start of snippet'>anchor</a></sup>
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

# Requirements

Tested by CI:

 * gRPC 1.37
 * Boost 1.77 (min. 1.74)
 * MSVC 19.29.30133.0 (Visual Studio 16 2019)
 * GCC 9.3.0, 10.3.0, 11.1.0
 * Clang 10.0.0, 11.0.0, 12.0.0
 * AppleClang 12.0.5.12050022
 * C++17 or C++20

For MSVC compilers the following compile definitions might need to be set:

```
BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT
```

# Usage

The library can be added to a CMake project using either `add_subdirectory` or `find_package` . Once set up, include the following header:

```c++
#include <agrpc/asioGrpc.hpp>
```

## As a subdirectory

Clone the repository into a subdirectory of your CMake project. Then add it and link it to your target.

```cmake
find_package(Boost)
find_package(gRPC)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc gRPC::grpc++ Boost::headers)
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
find_package(Boost)
find_package(gRPC)
# Make sure to set CMAKE_PREFIX_PATH to /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc gRPC::grpc++ Boost::headers)
```

### CMake Options

`ASIO_GRPC_USE_BOOST_CONTAINER` - Use Boost.Container instead of `<memory_resource>`

## Using vcpkg

Add [asio-grpc](https://github.com/microsoft/vcpkg/blob/master/ports/asio-grpc/vcpkg.json) to the dependencies inside your `vcpkg.json`: 

```json
{
    "name": "example",
    "version": "0.1.0",
    "dependencies": [
        "asio-grpc"
    ]
}
```

Locate asio-grpc and link it to your target in your `CMakeLists.txt`:

```cmake
find_package(Boost)
find_package(gRPC)
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc gRPC::grpc++ Boost::headers)
```

### Available features

`boost-container` - Use Boost.Container instead of `<memory_resource>`

See [selecting-library-features](https://vcpkg.io/en/docs/users/selecting-library-features.html) to learn how to select features with vcpkg.

# Performance

asio-grpc is part of [grpc_bench](https://github.com/LesnyRumcajs/grpc_bench). Head over there to compare its performance against other libraries and languages.

Results from the helloworld unary RPC   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, Boost 1.74, gRPC 1.41.0, asio-grpc v1.1.0, jemalloc 5.2.1

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_tonic_mt               |   47736 |       20.79 ms |        9.21 ms |       10.04 ms |      543.11 ms |   102.1% |     15.92 MiB |
| rust_grpcio                 |   42715 |       23.21 ms |       24.50 ms |       25.34 ms |       26.84 ms |   102.5% |     29.64 MiB |
| rust_thruster_mt            |   42404 |       23.42 ms |       10.32 ms |       11.19 ms |      599.79 ms |  100.81% |     13.56 MiB |
| cpp_grpc_mt                 |   41762 |       23.81 ms |       25.21 ms |       25.59 ms |       26.99 ms |  102.04% |     18.61 MiB |
| cpp_asio_grpc               |   38745 |       25.68 ms |       27.32 ms |       27.72 ms |       29.19 ms |  101.37% |     19.26 MiB |
| go_grpc                     |    7420 |      128.04 ms |      271.95 ms |      304.62 ms |      468.69 ms |  100.74% |     31.96 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_grpc_mt                 |   86099 |       10.04 ms |       17.88 ms |       21.87 ms |       29.62 ms |  204.02% |     55.54 MiB |
| cpp_asio_grpc               |   82149 |       10.62 ms |       19.86 ms |       23.71 ms |       34.37 ms |  202.55% |     51.03 MiB |
| rust_tonic_mt               |   75662 |       12.38 ms |       32.64 ms |       53.09 ms |       80.08 ms |  198.77% |     18.33 MiB |
| rust_thruster_mt            |   68533 |       13.77 ms |       37.44 ms |       57.56 ms |       85.80 ms |  200.76% |     14.38 MiB |
| rust_grpcio                 |   67688 |       14.19 ms |       21.56 ms |       24.30 ms |       28.24 ms |  203.04% |     36.89 MiB |
| go_grpc                     |   16270 |       53.21 ms |       98.04 ms |      112.21 ms |      182.18 ms |  153.74% |     29.48 MiB |

# Documentation

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [boost::asio::execution_context](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Boost.Asio functions that expect an `ExecutionContext` like [boost::asio::spawn](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/spawn/overload7.html).

Likewise, the `agrpc::GrpcExecutor` models the [Executor and Networking TS requirements](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and can therefore be used in places where Boost.Asio expects an `Executor`.

This library's API for RPCs is modeled closely after the asynchronous, tag-based API of gRPC. As an example, the equivalent for `grpc::ClientAsyncReader<helloworld::HelloReply>.Read(helloworld::HelloReply*, void*)` would be `agrpc::read(grpc::ClientAsyncReader<helloworld::HelloReply>&, helloworld::HelloReply&, CompletionToken)`. It can therefore be helpful to refer to [async_unary_call.h](https://github.com/grpc/grpc/blob/master/include/grpcpp/impl/codegen/async_unary_call.h) and [async_stream.h](https://github.com/grpc/grpc/blob/master/include/grpcpp/impl/codegen/async_stream.h) while working with this library.

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Boost.Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/use_awaitable.html), [std::future](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/use_future.html), [stackless coroutine](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/coroutine.html), [callback](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/executor_binder.html) and [Boost.Coroutine](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/basic_yield_context.html).

If you are interested in learning more about the implementation details of this library then check out [this blog article](https://medium.com/3yourmind/c-20-coroutines-for-asynchronous-grpc-services-5b3dab1d1d61).

## Getting started

Start by creating a `agrpc::GrpcContext`.

For servers and clients:

<!-- snippet: create-grpc_context-server-side -->
<a id='snippet-create-grpc_context-server-side'></a>
```cpp
grpc::ServerBuilder builder;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
```
<sup><a href='/example/example-server.cpp#L188-L191' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/example/example-client.cpp#L157-L159' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` (shown further below) and run it. Make sure to shutdown the `server` before destructing the `grpc_context`. Also destruct the `grpc_context` before destructing the `server`. A `grpc_context` can only be run on one thread at a time.

<!-- snippet: run-grpc_context-server-side -->
<a id='snippet-run-grpc_context-server-side'></a>
```cpp
grpc_context.run();
server->Shutdown();
}  // grpc_context is destructed here before the server
```
<sup><a href='/example/example-server.cpp#L204-L208' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
auto guard = boost::asio::make_work_guard(grpc_context);
```
<sup><a href='/example/example-client.cpp#L161-L163' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Alarm

gRPC provides a [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html) which similar to [boost::asio::steady_timer](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio/reference/steady_timer.html). Simply construct it and pass to it `agrpc::wait` with the desired deadline to wait for the specified amount of time without blocking the event loop.

<!-- snippet: alarm -->
<a id='snippet-alarm'></a>
```cpp
grpc::Alarm alarm;
bool wait_ok = agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(1), yield);
```
<sup><a href='/example/example-server.cpp#L27-L30' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L37-L43' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L45-L52' title='Snippet source file'>snippet source</a> | <a href='#snippet-unary-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L25-L30' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L31-L37' title='Snippet source file'>snippet source</a> | <a href='#snippet-unary-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L59-L64' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L66-L74' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L56-L62' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncWriter` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-client-streaming-client-side-alt -->
<a id='snippet-request-client-streaming-client-side-alt'></a>
```cpp
auto [writer, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context, response, yield);
```
<sup><a href='/example/example-client.cpp#L46-L49' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-client-side-alt' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L64-L74' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-streaming-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L81-L87' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L89-L98' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L93-L99' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncReader` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-server-streaming-client-side-alt -->
<a id='snippet-request-server-streaming-client-side-alt'></a>
```cpp
auto [reader, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, yield);
```
<sup><a href='/example/example-client.cpp#L83-L86' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-client-side-alt' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L101-L109' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L105-L110' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-server.cpp#L112-L125' title='Snippet source file'>snippet source</a> | <a href='#snippet-bidirectional-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L127-L132' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncReaderWriter` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-bidirectional-client-side-alt -->
<a id='snippet-request-bidirectional-client-side-alt'></a>
```cpp
auto [reader_writer, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub, client_context, yield);
```
<sup><a href='/example/example-client.cpp#L117-L120' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-client-side-alt' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/example/example-client.cpp#L134-L147' title='Snippet source file'>snippet source</a> | <a href='#snippet-bidirectional-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_metadata_ok`, `write_ok`, `writes_done_ok`, `read_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## Repeatedly request server-side

(**experimental**) The function `agrpc::repeatedly_request` helps to ensure that there are enough outstanding calls to `request` to match incoming RPCs. 
It takes the RPC, the Service and a copyable Handler as arguments and returns immediately. The Handler determines what to do with a client request, it could e.g. spawn a new coroutine to process it. 
The first argument passed to the Handler is a `agrpc::RPCRequestContext` - a move-only type that provides access to the `grpc::ServerContext`, the request (if any) 
and the responder that were used when requesting the call. The second argument is the result of the request - `true` indicates that the RPC has indeed been started. If the result is `false`, the server has been shutdown before this particular call got matched to an incoming RPC ([source](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a)).

The following example shows how to implement a generic Handler that spawns a new Boost.Coroutine for each incoming RPC and invokes 
the provided handler to process it.

<!-- snippet: repeatedly-request-spawner -->
<a id='snippet-repeatedly-request-spawner'></a>
```cpp
template <class Handler>
struct Spawner
{
    using executor_type = boost::asio::associated_executor_t<Handler>;
    using allocator_type = boost::asio::associated_allocator_t<Handler>;

    Handler handler;

    explicit Spawner(Handler handler) : handler(std::move(handler)) {}

    template <class T>
    void operator()(agrpc::RPCRequestContext<T>&& request_context, bool request_ok) &&
    {
        if (!request_ok)
        {
            return;
        }
        auto executor = this->get_executor();
        boost::asio::spawn(
            std::move(executor),
            [handler = std::move(handler),
             request_context = std::move(request_context)](const boost::asio::yield_context& yield) mutable
            {
                std::apply(std::move(handler), std::tuple_cat(request_context.args(), std::forward_as_tuple(yield)));
                // or
                std::invoke(std::move(request_context), std::move(handler), yield);
            });
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return boost::asio::get_associated_executor(handler); }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler);
    }
};

void repeatedly_request_example(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        Spawner{boost::asio::bind_executor(
            grpc_context.get_executor(),
            [&](grpc::ServerContext&, example::v1::Request&,
                grpc::ServerAsyncResponseWriter<example::v1::Response> writer, const boost::asio::yield_context& yield)
            {
                example::v1::Response response;
                agrpc::finish(writer, response, grpc::Status::OK, yield);
            })});
}
```
<sup><a href='/example/example-server.cpp#L130-L181' title='Snippet source file'>snippet source</a> | <a href='#snippet-repeatedly-request-spawner' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->
