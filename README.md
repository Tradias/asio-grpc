# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=coverage)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![vcpkg](https://repology.org/badge/version-for-repo/vcpkg/asio-grpc.svg?header=vcpkg)](https://repology.org/project/asio-grpc/versions)

A [Executor, Networking TS](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Unified Executors](https://brycelelbach.github.io/wg21_p2300_std_execution/std_execution.html) interface to [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html) for writing asynchronous gRPC clients and servers using C++20 coroutines, Boost.Coroutines, Asio's stackless coroutines, callbacks, sender/receiver and more.

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
* Add final completion handler and stop token support to `agrpc::repeatedly_request`
* Classes that wrap Stub or AsyncService as IO-Object, constructible from any [asio::execution_context](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution_context.html). Making it possible to write asynchronous gRPC clients and servers without explicit use of a `agrpc::GrpcContext`. Roughly as follows:

```cpp
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
 * [Boost](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio.html) 1.78 (min. 1.74 or [standalone Asio](https://github.com/chriskohlhoff/asio) 1.17.0)
 * MSVC 19.30.30706.0 (Visual Studio 17 2022)
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

```cpp
#include <agrpc/asioGrpc.hpp>
```

<details><summary><b>As a subdirectory</b></summary>
<p>

Clone the repository into a subdirectory of your CMake project. Then add it and link it to your target.

Using [Boost.Asio](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio.html):

```cmake
find_package(Boost)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc Boost::headers)
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
find_package(asio)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio asio::asio)
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
find_package(unifex)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-unifex unifex::unifex)
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
# Make sure CMAKE_PREFIX_PATH contains /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
# Make sure CMAKE_PREFIX_PATH contains /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio)
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
# Make sure CMAKE_PREFIX_PATH contains /desired/installation/directory
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

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/use_awaitable.html), [stackless coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/coroutine.html), [callback](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/executor_binder.html) and [Boost.Coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/basic_yield_context.html). There is also a special token created by `agrpc::use_sender(scheduler)` that causes RPC functions to return a [TypedSender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender).

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
<sup><a href='/doc/server.cpp#L237-L240' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/doc/client.cpp#L197-L199' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` (shown further below) and run it. Make sure to shutdown the `server` before destructing the `grpc_context`. Also destruct the `grpc_context` before destructing the `server`. A `grpc_context` can only be run on one thread at a time.

<!-- snippet: run-grpc_context-server-side -->
<a id='snippet-run-grpc_context-server-side'></a>
```cpp
grpc_context.run();
server->Shutdown();
}  // grpc_context is destructed here before the server
```
<sup><a href='/doc/server.cpp#L255-L259' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
auto guard = asio::make_work_guard(grpc_context);
```
<sup><a href='/doc/client.cpp#L201-L203' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## use_sender

A special completion token created by `agrpc::use_sender(scheduler)` where `scheduler` is a `agrpc::GrpcContext` or `agrpc::GrpcExecutor`. It causes RPC step functions to return a [TypedSender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender). The sender can e.g. be connected to a [unifex::task<>](https://github.com/facebookexperimental/libunifex/blob/main/doc/api_reference.md#task) to await completion of the RPC step:

<!-- snippet: unifex-server-streaming-client-side -->
<a id='snippet-unifex-server-streaming-client-side'></a>
```cpp
unifex::task<void> unified_executors(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    co_await agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, reader,
                            agrpc::use_sender(grpc_context));
    example::v1::Response response;
    co_await agrpc::read(*reader, response, agrpc::use_sender(grpc_context));
    grpc::Status status;
    co_await agrpc::finish(*reader, status, agrpc::use_sender(grpc_context));
}
```
<sup><a href='/doc/unifex-client.cpp#L25-L38' title='Snippet source file'>snippet source</a> | <a href='#snippet-unifex-server-streaming-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Different completion tokens

The last argument to all async functions in this library is a [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). It can be used to customize how to receive notification of the completion of the asynchronous operation. Aside from the ones shown earlier (`asio::yield_context` and `agrpc::use_sender`) there are many more, some examples:

### Callback

<!-- snippet: alarm-with-callback -->
<a id='snippet-alarm-with-callback'></a>
```cpp
agrpc::wait(alarm, deadline, asio::bind_executor(grpc_context, [&](bool /*wait_ok*/) {}));
```
<sup><a href='/doc/server.cpp#L45-L47' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm-with-callback' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

### Stackless coroutine

<!-- snippet: alarm-stackless-coroutine -->
<a id='snippet-alarm-stackless-coroutine'></a>
```cpp
struct Coro : asio::coroutine
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
            BOOST_ASIO_CORO_YIELD agrpc::wait(context->alarm, context->deadline, std::move(*this));
            (void)wait_ok;
        }
    }

    executor_type get_executor() const noexcept { return context->grpc_context.get_executor(); }
};
Coro{deadline, grpc_context}(false);
```
<sup><a href='/doc/server.cpp#L49-L85' title='Snippet source file'>snippet source</a> | <a href='#snippet-alarm-stackless-coroutine' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Repeatedly request server-side

The function `agrpc::repeatedly_request` helps to ensure that there are enough outstanding calls to `request` to match incoming RPCs. 
It takes a RPC, a Service, a Handler and a CompletionToken. The Handler determines what to do with a client request, it could e.g. spawn a new coroutine to process it. It must also have an associated executor that refers to a `agrpc::GrpcContext`. When the client makes a request the Handler is invoked with a `agrpc::RepeatedlyRequestContext` - a move-only type that provides a stable address to the `grpc::ServerContext`, the request (if any) and the responder that were used when requesting the RPC. It should be kept alive until the RPC is finished. The Handler or its associated executor may also have an associated allocator to control the allocation needed for each request.

When using the special CompletionToken created by `agrpc::use_sender` the Handler's signature must be:    
`sender auto operator()(grpc::ServerContext&, Request&, Responder&)` for unary and server-streaming requests and   
`sender auto operator()(grpc::ServerContext&, Responder&)` otherwise.

Another special overload of `agrpc::repeatedly_request` can be used by passing a Handler with the following signature:   
`asio::awaitable<void, Executor> operator()(grpc::ServerContext&, Request&, Responder&)` for unary and server-streaming requests or   
`asio::awaitable<void, Executor> operator()(grpc::ServerContext&, Responder&)` otherwise.

`agrpc::repeatedly_request` will complete when it was cancelled, the `agrpc::GrpcContext` was stopped or the `grpc::Server` been shutdown. It will **not** wait until all outstanding RPCs that are being processed by the Handler have completed.

The following example shows how to implement a generic Handler with a custom allocator for simple, high-performance RPC processing. More examples can be found in  the `/example` directory.

<!-- snippet: repeatedly-request-callback -->
<a id='snippet-repeatedly-request-callback'></a>
```cpp
template <class Executor, class Handler>
struct AssociatedHandler
{
    using executor_type = Executor;

    Executor executor;
    /*[[no_unique_address]]*/ Handler handler;

    AssociatedHandler(Executor executor, Handler handler) : executor(std::move(executor)), handler(std::move(handler))
    {
    }

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& request_context)
    {
        std::invoke(handler, std::move(request_context), executor);
        //
        // The RepeatedlyRequestContext also provides access to:
        // * the grpc::ServerContext
        // request_context.server_context();
        // * the grpc::ServerAsyncReader/Writer
        // request_context.responder();
        // * the protobuf request message (for unary and server-streaming requests)
        // request_context.request();
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }
};

void repeatedly_request_example(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        AssociatedHandler{
            asio::require(grpc_context.get_executor(), asio::execution::allocator(grpc_context.get_allocator())),
            [](auto&& request_context, auto&& executor)
            {
                auto& writer = request_context.responder();
                example::v1::Response response;
                agrpc::finish(writer, response, grpc::Status::OK,
                              asio::bind_executor(executor, [c = std::move(request_context)](bool) {}));
            }});
}
```
<sup><a href='/doc/server.cpp#L186-L230' title='Snippet source file'>snippet source</a> | <a href='#snippet-repeatedly-request-callback' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## CMake asio_grpc_protobuf_generate 

In the same directory that called `find_package(asio-grpc)` a function called `asio_grpc_protobuf_generate` is made available. It can be used to generate Protobuf/gRPC source files from `.proto` files:

<!-- snippet: asio_grpc_protobuf_generate-target -->
<a id='snippet-asio_grpc_protobuf_generate-target'></a>
```cmake
asio_grpc_protobuf_generate(
    GENERATE_GRPC
    TARGET target-option
    OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/target"
    PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/proto/target.proto")
```
<sup><a href='/test/cmake/Targets.cmake#L36-L42' title='Snippet source file'>snippet source</a> | <a href='#snippet-asio_grpc_protobuf_generate-target' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

See in-code documentation for more details:

<!-- snippet: asio_grpc_protobuf_generate -->
<a id='snippet-asio_grpc_protobuf_generate'></a>
```cmake
function(asio_grpc_protobuf_generate)
```
<sup><a href='/cmake/AsioGrpcProtobufGenerator.cmake#L53-L55' title='Snippet source file'>snippet source</a> | <a href='#snippet-asio_grpc_protobuf_generate' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

If you are using [cmake-format](https://github.com/cheshirekow/cmake_format) then you can copy the `asio_grpc_protobuf_generate` section from [cmake-format.yaml](cmake-format.yaml#L2-L13) into your cmake-format.yaml to get proper formatting.

</p>
</details>


# What users are saying

> Asio-grpc abstracts away the implementation details of asynchronous grpc handling: crafting working code is easier, faster, less prone to errors and considerably more fun. At 3YOURMIND we reliably use asio-grpc in production since its very first release, allowing our developers to effortlessly implement low-latency/high-throughput asynchronous data transfer in time critical applications.

[@3YOURMIND](https://github.com/3YOURMIND)

> Our project is a real-time distributed motion capture system that uses your framework to stream data back and forward between multiple machines. Previously I have tried to build a bidirectional streaming framework from scratch using only gRPC. However, it's not maintainable and error-prone due to a large amount of service and streaming code. As a developer whose experienced both raw grpc and asio-grpc, I can tell that your framework is a real a game-changer for writing grpc code in C++. It has made my life much easier. I really appreciate the effort you have put into this project and your superior skills in designing c++ template code.

[@khanhha](https://github.com/khanhha)
