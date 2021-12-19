# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=coverage)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![vcpkg package](https://repology.org/badge/version-for-repo/vcpkg/asio-grpc.svg?header=vcpkg%20package)](https://repology.org/project/asio-grpc/versions)

A [Executor, Networking TS](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Unified Executors](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r13.html) interface to [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html) for writing asynchronous gRPC clients and servers using C++20 coroutines, Boost.Coroutines, Asio's stackless coroutines or callbacks.

# Features and Roadmap

Completed features:

* Asio [ExecutionContext](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/ExecutionContext.html) compatible wrapper around [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html)
* [Executor and Networking TS requirements](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) fulfilling associated executor
* Support for all RPC types: unary, client-streaming, server-streaming and bidirectional-streaming with any mix of Asio [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers) as well as  [TypedSender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept)
* Support for asynchronously waiting for [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html)s including cancellation through [cancellation_slot](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/cancellation_slot.html)s
* Initial support for unified executor concepts through [libunifex](https://github.com/facebookexperimental/libunifex) and Asio: [schedule](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__schedule.html), [connect](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__connect.html), [submit](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__submit.html), [scheduler](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Scheduler.html), [typed_sender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender) and more
* No-Boost version with [standalone Asio](https://github.com/chriskohlhoff/asio)
* No-Asio version with [libunifex](https://github.com/facebookexperimental/libunifex)
* CMake function for easily running `protoc`: [asio_grpc_protobuf_generate](/cmake/AsioGrpcProtobufGenerator.cmake)

Upcoming features for v1.4.0:

* [CancellationSlot](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/CancellationSlot.html) and [StopToken](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#stoptoken-concept) support for individual RPC steps
* [AsyncStream](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#streams) for streaming RPCs
* Classes that wrap Stub or AsyncService as IO-Object, constructible from any [asio::execution_context](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution_context.html). Making it possible to write asynchronous gRPC clients and servers without explicit use of a `agrpc::GrpcContext`. Roughly as follows:

```c++
const auto stub = test::v1::Test::NewStub(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
boost::asio::io_context io_context;
agrpc::GrpcStub grpc_stub{*stub, io_context};
co_await agrpc::request(..., grpc_stub, ...);
```

# Example

* Server side 'hello world':

<!-- snippet: server-side-helloworld -->
<a id='snippet-server-side-helloworld'></a>
```cpp
grpc::ServerBuilder builder;
std::unique_ptr<grpc::Server> server;
helloworld::Greeter::AsyncService service;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
builder.AddListeningPort(host, grpc::InsecureServerCredentials());
builder.RegisterService(&service);
server = builder.BuildAndStart();

boost::asio::co_spawn(
    grpc_context,
    [&]() -> boost::asio::awaitable<void>
    {
        grpc::ServerContext server_context;
        helloworld::HelloRequest request;
        grpc::ServerAsyncResponseWriter<helloworld::HelloReply> writer{&server_context};
        co_await agrpc::request(&helloworld::Greeter::AsyncService::RequestSayHello, service, server_context,
                                request, writer);
        helloworld::HelloReply response;
        response.set_message("Hello " + request.name());
        co_await agrpc::finish(writer, response, grpc::Status::OK);
    },
    boost::asio::detached);

grpc_context.run();
```
<sup><a href='/example/hello-world-server.cpp#L32-L57' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

* Client side 'hello world':

<!-- snippet: client-side-helloworld -->
<a id='snippet-client-side-helloworld'></a>
```cpp
const auto stub = helloworld::Greeter::NewStub(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
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
        co_await agrpc::finish(*reader, response, status);
    },
    boost::asio::detached);

grpc_context.run();
```
<sup><a href='/example/hello-world-client.cpp#L31-L50' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

* Boost.Asio [client](/example/streaming-client.cpp) and [server](/example/streaming-server.cpp) streaming RPCs

* libunifex based [client](/example/unifex-client.cpp) and [server](/example/unifex-server.cpp)

# Requirements

Tested by CI:

 * gRPC 1.41.0 (older versions work as well)
 * [Boost](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio.html) 1.77 (min. 1.74 or [standalone Asio](https://github.com/chriskohlhoff/asio) 1.17.0)
 * MSVC 19.29.30137.0 (Visual Studio 17 2022)
 * GCC 9.3.0, 10.3.0, 11.1.0
 * Clang 10.0.0, 11.0.0, 12.0.0
 * AppleClang 13.0.0.13000029
 * C++17 or C++20

For MSVC compilers the following compile definitions might need to be set:

```
BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT
```

When using [standalone Asio](https://github.com/chriskohlhoff/asio) then omit the `BOOST_` prefix.

# Usage

The library can be added to a CMake project using either `add_subdirectory` or `find_package`. Once set up, include the following header:

```c++
#include <agrpc/asioGrpc.hpp>
```

<details><summary><b>As a subdirectory</b></summary>
<p>

Clone the repository into a subdirectory of your CMake project. Then add it and link it to your target.

Using [Boost.Asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html):

```cmake
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio)
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-unifex)
```

</p>
</details>

<details><summary><b>As a CMake package</b></summary>
<p>

Clone the repository and install it.

```shell
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/desired/installation/directory ..
cmake --build . --target install
```

Locate it and link it to your target.

Using [Boost.Asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html):

```cmake
# Make sure to set CMAKE_PREFIX_PATH to /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
# Make sure to set CMAKE_PREFIX_PATH to /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio)
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
# Make sure to set CMAKE_PREFIX_PATH to /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-unifex)
```

</p>
</details>

<details><summary><b>Using vcpkg</b></summary>
<p>

Add [asio-grpc](https://github.com/microsoft/vcpkg/blob/master/ports/asio-grpc/vcpkg.json) to the dependencies inside your `vcpkg.json`: 

```json
{
    "name": "your_app",
    "version": "0.1.0",
    "dependencies": [
        "asio-grpc",
        // To use the Boost.Asio backend add
        // "boost-asio",
        // To use the standalone Asio backend add
        // "asio",
        // To use the libunifex backend add
        // "libunifex"
    ]
}
```

Locate asio-grpc and link it to your target in your `CMakeLists.txt`:

```cmake
find_package(asio-grpc)
# Using the Boost.Asio backend
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
# Or use the standalone Asio backend
#target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio)
# Or use the libunifex backend
#target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-unifex)
```

### Available features

`boost-container` - Use Boost.Container instead of `<memory_resource>`

See [selecting-library-features](https://vcpkg.io/en/docs/users/selecting-library-features.html) to learn how to select features with vcpkg.

</p>
</details>

## CMake Options

`ASIO_GRPC_USE_BOOST_CONTAINER` - Use Boost.Container instead of `<memory_resource>`

`ASIO_GRPC_DISABLE_AUTOLINK` - Set before using `find_package(asio-grpc)` to prevent `asio-grpcConfig.cmake` from finding and setting up interface link libraries

# Performance

asio-grpc is part of [grpc_bench](https://github.com/Tradias/grpc_bench). Head over there to compare its performance against other libraries and languages.

Results from the helloworld unary RPC   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, Boost 1.74, gRPC 1.41.0, asio-grpc v1.3.0, jemalloc 5.2.1

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_tonic_mt               |   47805 |       20.75 ms |        9.08 ms |        9.95 ms |      563.63 ms |  101.47% |     30.57 MiB |
| rust_thruster_mt            |   42444 |       23.41 ms |       10.20 ms |       11.10 ms |      618.14 ms |  100.88% |      22.4 MiB |
| rust_grpcio                 |   41832 |       23.71 ms |       25.21 ms |       26.04 ms |       27.40 ms |  102.47% |     46.52 MiB |
| cpp_grpc_mt                 |   40744 |       24.40 ms |       25.87 ms |       26.45 ms |       28.27 ms |  101.56% |     18.47 MiB |
| cpp_asio_grpc libunifex        |   40736 |       24.41 ms |       25.90 ms |       26.38 ms |       28.01 ms |  101.31% |     20.03 MiB |
| cpp_asio_grpc Boost.Coroutine |   40131 |       24.78 ms |       26.40 ms |       27.06 ms |       28.53 ms |  101.23% |     21.62 MiB |
| cpp_asio_grpc C++20 coroutines |   39301 |       25.31 ms |       27.15 ms |       27.86 ms |       30.17 ms |  101.56% |     18.73 MiB |
| cpp_grpc_callback           |   12295 |       76.83 ms |      103.27 ms |      111.26 ms |      157.36 ms |   99.13% |    122.13 MiB |
| go_grpc                     |    7460 |      127.03 ms |      233.60 ms |      298.85 ms |      476.07 ms |   76.98% |     31.17 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_asio_grpc libunifex        |   85160 |       10.16 ms |       18.48 ms |       22.30 ms |       30.35 ms |  199.12% |     47.49 MiB |
| cpp_asio_grpc Boost.Coroutine |   83983 |       10.35 ms |       18.44 ms |       22.52 ms |       32.10 ms |  202.86% |     52.52 MiB |
| cpp_grpc_mt                 |   83662 |       10.34 ms |       18.79 ms |       23.12 ms |       33.93 ms |  200.63% |     50.81 MiB |
| cpp_asio C++20 coroutines |   83269 |       10.46 ms |       18.90 ms |       22.81 ms |       30.87 ms |  200.28% |     46.97 MiB |
| cpp_grpc_callback           |   78264 |       11.21 ms |       18.83 ms |       23.75 ms |       35.76 ms |   205.3% |    156.57 MiB |
| rust_tonic_mt               |   76169 |       12.30 ms |       32.65 ms |       52.59 ms |       79.94 ms |  199.34% |     18.65 MiB |
| rust_thruster_mt            |   68978 |       13.68 ms |       37.60 ms |       58.65 ms |       86.11 ms |  201.22% |     14.56 MiB |
| rust_grpcio                 |   67483 |       14.26 ms |       20.94 ms |       23.91 ms |       28.20 ms |  201.54% |     39.61 MiB |
| go_grpc                     |   15983 |       54.77 ms |      101.33 ms |      119.37 ms |      188.73 ms |  196.62% |     30.62 MiB |

</p>
</details>

# Documentation

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [asio::execution_context](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Asio functions that expect an `ExecutionContext` like [asio::spawn](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/spawn/overload7.html).

Likewise, the `agrpc::GrpcExecutor` models the [Executor and Networking TS requirements](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Scheduler](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Scheduler.html) and can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.

The API for RPCs is modeled closely after the asynchronous, tag-based API of gRPC. As an example, the equivalent for `grpc::ClientAsyncReader<helloworld::HelloReply>.Read(helloworld::HelloReply*, void*)` would be `agrpc::read(grpc::ClientAsyncReader<helloworld::HelloReply>&, helloworld::HelloReply&, CompletionToken)`. It can therefore be helpful to refer to [async_unary_call.h](https://github.com/grpc/grpc/blob/master/include/grpcpp/impl/codegen/async_unary_call.h) and [async_stream.h](https://github.com/grpc/grpc/blob/master/include/grpcpp/impl/codegen/async_stream.h) while working with this library.

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/use_awaitable.html), [stackless coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/coroutine.html), [callback](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/executor_binder.html) and [Boost.Coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/basic_yield_context.html). There is also a special token created by `agrpc::use_scheduler(scheduler)` that causes RPC functions to returns a [TypedSender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender).

If you are interested in learning more about the implementation details of this library then check out [this blog article](https://medium.com/3yourmind/c-20-coroutines-for-asynchronous-grpc-services-5b3dab1d1d61).

<details><summary><b>Click to see full documentation</b></summary>
<p>

## Getting started

Start by creating a `agrpc::GrpcContext`.

For servers and clients:

<!-- snippet: create-grpc_context-server-side -->
<a id='snippet-create-grpc_context-server-side'></a>
```cpp
grpc::ServerBuilder builder;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
```
<sup><a href='/doc/server.cpp#L258-L261' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/doc/client.cpp#L157-L159' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` (shown further below) and run it. Make sure to shutdown the `server` before destructing the `grpc_context`. Also destruct the `grpc_context` before destructing the `server`. A `grpc_context` can only be run on one thread at a time.

<!-- snippet: run-grpc_context-server-side -->
<a id='snippet-run-grpc_context-server-side'></a>
```cpp
grpc_context.run();
server->Shutdown();
}  // grpc_context is destructed here before the server
```
<sup><a href='/doc/server.cpp#L274-L278' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
auto guard = boost::asio::make_work_guard(grpc_context);
```
<sup><a href='/doc/client.cpp#L161-L163' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Alarm

gRPC provides a [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html) which similar to [asio::steady_timer](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/steady_timer.html). Simply construct it and pass to it `agrpc::wait` with the desired deadline to wait for the specified amount of time without blocking the event loop.

<!-- snippet: alarm -->
<a id='snippet-alarm'></a>
```cpp
grpc::Alarm alarm;
bool wait_ok = agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(1), yield);
```
<sup><a href='/doc/server.cpp#L29-L32' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

`wait_ok` is true if the Alarm expired, false if it was canceled. ([source](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a))

## Unary RPC Server-Side

Start by requesting a RPC. In this example `yield` is a [asio::yield_context](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/yield_context.html), other [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers)s are supported as well, e.g. [asio::use_awaitable](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/use_awaitable.html). The `example` namespace has been generated from [example.proto](/example/protos/example.proto).

<!-- snippet: request-unary-server-side -->
<a id='snippet-request-unary-server-side'></a>
```cpp
grpc::ServerContext server_context;
example::v1::Request request;
grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service, server_context,
                                 request, writer, yield);
```
<sup><a href='/doc/server.cpp#L97-L103' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/server.cpp#L105-L112' title='Snippet source file'>snippet source</a> | <a href='#snippet-unary-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L25-L30' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-unary-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L31-L37' title='Snippet source file'>snippet source</a> | <a href='#snippet-unary-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/server.cpp#L119-L124' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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

bool finish_with_error_ok = agrpc::finish_with_error(reader, grpc::Status::CANCELLED, yield);
```
<sup><a href='/doc/server.cpp#L126-L136' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L56-L62' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncWriter` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-client-streaming-client-side-alt -->
<a id='snippet-request-client-streaming-client-side-alt'></a>
```cpp
auto [writer, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context, response, yield);
```
<sup><a href='/doc/client.cpp#L46-L49' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-client-streaming-client-side-alt' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L64-L74' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-streaming-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/server.cpp#L143-L149' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/server.cpp#L151-L160' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L93-L99' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncReader` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-server-streaming-client-side-alt -->
<a id='snippet-request-server-streaming-client-side-alt'></a>
```cpp
auto [reader, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, yield);
```
<sup><a href='/doc/client.cpp#L83-L86' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-server-streaming-client-side-alt' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L101-L109' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/server.cpp#L167-L172' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/server.cpp#L174-L187' title='Snippet source file'>snippet source</a> | <a href='#snippet-bidirectional-streaming-server-side' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L127-L132' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

There is also a convenience overload that returns the `grpc::ClientAsyncReaderWriter` at the cost of a `sizeof(std::unique_ptr)` memory overhead.

<!-- snippet: request-bidirectional-client-side-alt -->
<a id='snippet-request-bidirectional-client-side-alt'></a>
```cpp
auto [reader_writer, request_ok] =
    agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub, client_context, yield);
```
<sup><a href='/doc/client.cpp#L117-L120' title='Snippet source file'>snippet source</a> | <a href='#snippet-request-bidirectional-client-side-alt' title='Start of snippet'>anchor</a></sup>
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
<sup><a href='/doc/client.cpp#L134-L147' title='Snippet source file'>snippet source</a> | <a href='#snippet-bidirectional-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For the meaning of `read_metadata_ok`, `write_ok`, `writes_done_ok`, `read_ok` and `finish_ok` see [CompletionQueue::Next](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a).

## use_scheduler

A special completion token created by `agrpc::use_scheduler(scheduler)` where `scheduler` is a `agrpc::GrpcContext` or `agrpc::GrpcExecutor`. It causes RPC step functions to return a [TypedSender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender). The sender can e.g. be connected to a [unifex::task<>](https://github.com/facebookexperimental/libunifex/blob/main/doc/api_reference.md#task) to await completion of the RPC step:

<!-- snippet: unifex-server-streaming-client-side -->
<a id='snippet-unifex-server-streaming-client-side'></a>
```cpp
unifex::task<void> unified_executors(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    test::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<test::v1::Response>> reader;
    co_await agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, stub, client_context, request, reader,
                            agrpc::use_scheduler(grpc_context));
    test::v1::Response response;
    co_await agrpc::read(*reader, response, agrpc::use_scheduler(grpc_context));
    grpc::Status status;
    co_await agrpc::finish(*reader, status, agrpc::use_scheduler(grpc_context));
}
```
<sup><a href='/doc/unifex-client.cpp#L25-L38' title='Snippet source file'>snippet source</a> | <a href='#snippet-unifex-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Different completion tokens

The last argument to all async functions in this library is a [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). It can be used to customize how to receive notification of the completion of the asynchronous operation. Aside from the ones shown earlier (`asio::yield_context` and `agrpc::use_scheduler`) there are many more, some examples:

### Callback

<!-- snippet: alarm-with-callback -->
<a id='snippet-alarm-with-callback'></a>
```cpp
agrpc::wait(alarm, deadline, boost::asio::bind_executor(grpc_context, [&](bool /*wait_ok*/) {}));
```
<sup><a href='/doc/server.cpp#L41-L43' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm-with-callback' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

### Stackless coroutine

<!-- snippet: alarm-stackless-coroutine -->
<a id='snippet-alarm-stackless-coroutine'></a>
```cpp
struct Coro : boost::asio::coroutine
{
    using executor_type = agrpc::GrpcContext::executor_type;

    struct Context
    {
        std::chrono::system_clock::time_point deadline;
        agrpc::GrpcContext& grpc_context;
        grpc::Alarm alarm;

        Context(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context)
            : deadline(deadline), grpc_context(grpc_context)
        {
        }
    };

    std::shared_ptr<Context> context;

    Coro(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context)
        : context(std::make_shared<Context>(deadline, grpc_context))
    {
    }

    void operator()(bool wait_ok)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD agrpc::wait(context->alarm, context->deadline, *this);
            (void)wait_ok;
        }
    }

    executor_type get_executor() const noexcept { return context->grpc_context.get_executor(); }
};
Coro{deadline, grpc_context}(false);
```
<sup><a href='/doc/server.cpp#L45-L81' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm-stackless-coroutine' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

### Experimental deferred

<!-- snippet: alarm-double-deferred -->
<a id='snippet-alarm-double-deferred'></a>
```cpp
auto deferred_op = agrpc::wait(alarm, deadline,
                               boost::asio::experimental::deferred(
                                   [&](bool /*wait_ok*/)
                                   {
                                       return agrpc::wait(alarm, deadline + std::chrono::seconds(1),
                                                          boost::asio::experimental::deferred);
                                   }));
std::move(deferred_op)(yield);
```
<sup><a href='/doc/server.cpp#L83-L92' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm-double-deferred' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Repeatedly request server-side

(**experimental**) The function `agrpc::repeatedly_request` helps to ensure that there are enough outstanding calls to `request` to match incoming RPCs. 
It takes the RPC, the Service and a copyable Handler as arguments and returns immediately. The Handler determines what to do with a client request, it could e.g. spawn a new coroutine to process it. 
The first argument passed to the Handler is a `agrpc::RepeatedlyRequestContext` - a move-only type that provides a stable address to the `grpc::ServerContext`, the request (if any) 
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
    void operator()(agrpc::RepeatedlyRequestContext<T>&& request_context, bool request_ok) &&
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
                // Or
                // std::invoke(std::move(request_context), std::move(handler), yield);
                //
                // The RepeatedlyRequestContext also provides access to:
                // * the grpc::ServerContext
                // request_context.server_context();
                // * the grpc::ServerAsyncReader/Writer
                // request_context.responder();
                // * the protobuf request message (for unary and server-streaming requests)
                // request_context.request();
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
            grpc_context,
            [&](grpc::ServerContext&, example::v1::Request&,
                grpc::ServerAsyncResponseWriter<example::v1::Response> writer, const boost::asio::yield_context& yield)
            {
                example::v1::Response response;
                agrpc::finish(writer, response, grpc::Status::OK, yield);
            })});
}
```
<sup><a href='/doc/server.cpp#L192-L251' title='Snippet source file'>snippet source</a> | <a href='#snippet-repeatedly-request-spawner' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## CMake asio_grpc_protobuf_generate 

In the same directory that called `find_package(asio-grpc)` a function called `asio_grpc_protobuf_generate` is made available. It can be used to generate Protobuf/gRPC source files from `.proto` files:

<!-- snippet: asio_grpc_protobuf_generate-target -->
<a id='snippet-asio_grpc_protobuf_generate-target'></a>
```cmake
set(TARGET_GENERATED_PROTOS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")

asio_grpc_protobuf_generate(
    GENERATE_GRPC
    TARGET target-option
    OUT_DIR "${TARGET_GENERATED_PROTOS_OUT_DIR}"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/target.proto")

target_include_directories(target-option PRIVATE "${TARGET_GENERATED_PROTOS_OUT_DIR}")
```
<sup><a href='/test/cmake/Targets.cmake#L37-L47' title='Snippet source file'>snippet source</a> | <a href='#snippet-asio_grpc_protobuf_generate-target' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

See in-code documentation for more details:

<!-- snippet: asio_grpc_protobuf_generate -->
<a id='snippet-asio_grpc_protobuf_generate'></a>
```cmake
function(asio_grpc_protobuf_generate)
```
<sup><a href='/cmake/AsioGrpcProtobufGenerator.cmake#L53-L55' title='Snippet source file'>snippet source</a> | <a href='#snippet-asio_grpc_protobuf_generate' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

If you are using [cmake-format](https://github.com/cheshirekow/cmake_format) then you can copy the `asio_grpc_protobuf_generate` section from [cmake-format.yaml](cmake-format.yaml#L1-L12) into your cmake-format.yaml to get proper formatting.

</p>
</details>
