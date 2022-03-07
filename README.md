# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=coverage)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![vcpkg](https://repology.org/badge/version-for-repo/vcpkg/asio-grpc.svg?header=vcpkg)](https://repology.org/project/asio-grpc/versions)

A [Executor, Networking TS](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Unified Executors](https://brycelelbach.github.io/wg21_p2300_std_execution/std_execution.html) interface to [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html) for writing asynchronous gRPC clients and servers using C++20 coroutines, Boost.Coroutines, Asio's stackless coroutines, callbacks, sender/receiver and more.

# Features

* Asio [ExecutionContext](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/ExecutionContext.html) compatible wrapper around [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html)
* [Executor and Networking TS](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) requirements fulfilling associated executor
* Support for all RPC types: unary, client-streaming, server-streaming and bidirectional-streaming with any mix of Asio [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers) as well as  [TypedSender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept)
* Support for asynchronously waiting for [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html)s including cancellation through [cancellation_slot](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/cancellation_slot.html)s and [StopToken](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#stoptoken-concept)s
* Initial support for unified executor concepts through [libunifex](https://github.com/facebookexperimental/libunifex) and Asio: [schedule](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__schedule.html), [connect](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__connect.html), [submit](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__submit.html), [scheduler](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Scheduler.html), [typed_sender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender) and more
* No-Boost version with [standalone Asio](https://github.com/chriskohlhoff/asio)
* No-Asio version with [libunifex](https://github.com/facebookexperimental/libunifex)
* CMake function to generate gRPC source files: [asio_grpc_protobuf_generate](/cmake/AsioGrpcProtobufGenerator.cmake)

# Example

* Server side 'hello world':

<!-- snippet: server-side-helloworld -->
<a id='snippet-server-side-helloworld'></a>
```cpp
std::unique_ptr<grpc::Server> server;

grpc::ServerBuilder builder;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
builder.AddListeningPort(host, grpc::InsecureServerCredentials());
helloworld::Greeter::AsyncService service;
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
<sup><a href='/example/hello-world-server.cpp#L32-L58' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

* [Client side hello world](/example/hello-world-client.cpp)

* Boost.Asio based streaming [client](/example/streaming-client.cpp) and [server](/example/streaming-server.cpp)

* libunifex based [client](/example/unifex-client.cpp) and [server](/example/unifex-server.cpp)

# Requirements

Tested by CI:

 * gRPC 1.41.0 (older versions work as well)
 * [Boost](https://www.boost.org/doc/libs/1_77_0/doc/html/boost_asio.html) 1.78 (min. 1.74 or [standalone Asio](https://github.com/chriskohlhoff/asio) 1.17.0)
 * MSVC 19.29.30140.0 (Visual Studio 16 2019)
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

The library can be added to a CMake project using either `add_subdirectory` or `find_package`. Once set up, include the individual headers from the agrpc/ directory or the combined header:

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
cmake -B build -DCMAKE_INSTALL_PREFIX=/desired/installation/directory .
cmake --build build --target install
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

```
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

`ASIO_GRPC_USE_BOOST_CONTAINER` - Use Boost.Container instead of `<memory_resource>`.

`ASIO_GRPC_DISABLE_AUTOLINK` - Set before using `find_package(asio-grpc)` to prevent `asio-grpcConfig.cmake` from finding and setting up interface link libraries.

# Performance

asio-grpc is part of [grpc_bench](https://github.com/Tradias/grpc_bench). Head over there to compare its performance against other libraries and languages.

Results from the helloworld unary RPC   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, Boost 1.78, gRPC 1.43.2, asio-grpc v1.4.0, jemalloc 5.2.1

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_tonic_mt               |   47801 |       20.76 ms |        9.05 ms |        9.90 ms |      565.77 ms |  103.78% |     15.86 MiB |
| rust_thruster_mt            |   42167 |       23.57 ms |       10.49 ms |       11.30 ms |      632.77 ms |  101.34% |     13.22 MiB |
| rust_grpcio                 |   41191 |       24.09 ms |       25.59 ms |       26.33 ms |       28.97 ms |  101.41% |     25.29 MiB |
| cpp_grpc_mt                 |   41110 |       24.19 ms |       25.76 ms |       26.37 ms |       27.87 ms |  101.86% |     20.31 MiB |
| cpp_asio_grpc_unifex        |   40653 |       24.46 ms |       26.02 ms |       26.59 ms |       28.36 ms |  101.45% |     23.61 MiB |
| cpp_asio_grpc_callback |   40240 |       24.72 ms |       26.30 ms |       26.87 ms |       28.48 ms |  101.72% |     27.62 MiB |
| cpp_asio_grpc_cpp20_coroutine |   35288 |       28.21 ms |       30.55 ms |       31.07 ms |       32.73 ms |  102.02% |     20.32 MiB |
| cpp_grpc_callback           |   11931 |       79.32 ms |      105.00 ms |      113.98 ms |      166.25 ms |  101.12% |    117.23 MiB |
| go_grpc                     |    7330 |      129.66 ms |      276.93 ms |      307.12 ms |      487.44 ms |   76.31% |     30.06 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_grpc_mt                 |   82457 |       10.37 ms |       19.13 ms |       23.20 ms |       33.10 ms |  201.19% |     52.21 MiB |
| cpp_asio_grpc_unifex        |   82365 |       10.36 ms |       19.59 ms |       23.97 ms |       32.94 ms |  201.42% |      58.2 MiB |
| cpp_asio_grpc_callback |   81589 |       10.66 ms |       19.84 ms |       23.64 ms |       31.93 ms |  200.18% |     50.16 MiB |
| rust_tonic_mt               |   74778 |       12.55 ms |       33.45 ms |       54.19 ms |       82.28 ms |  200.51% |     18.15 MiB |
| cpp_asio_grpc_cpp20_coroutine |   72049 |       12.49 ms |       24.33 ms |       29.38 ms |       42.77 ms |  198.98% |     52.79 MiB |
| cpp_grpc_callback           |   71035 |       12.40 ms |       24.69 ms |       30.73 ms |       45.59 ms |  203.35% |    158.57 MiB |
| rust_thruster_mt            |   67819 |       13.92 ms |       38.39 ms |       58.63 ms |       88.22 ms |  196.56% |     14.67 MiB |
| rust_grpcio                 |   67716 |       14.19 ms |       21.48 ms |       24.27 ms |       28.35 ms |  199.52% |     40.12 MiB |
| go_grpc                     |   15172 |       57.59 ms |      102.37 ms |      119.82 ms |      190.65 ms |  152.14% |     29.29 MiB |

</p>
</details>

# Documentation

[**API reference**](https://tradias.github.io/asio-grpc/)

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [asio::execution_context](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Asio functions that expect an `ExecutionContext` like [asio::spawn](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/spawn/overload7.html).

Likewise, the `agrpc::GrpcExecutor` satisfies the [Executor and Networking TS](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Scheduler](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Scheduler.html) requirements and can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.

The API for RPCs is modeled closely after the asynchronous, tag-based API of gRPC. As an example, the equivalent for `grpc::ClientAsyncReader<helloworld::HelloReply>.Read(helloworld::HelloReply*, void*)` would be `agrpc::read(grpc::ClientAsyncReader<helloworld::HelloReply>&, helloworld::HelloReply&, CompletionToken)`.

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/use_awaitable.html), [stackless coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/coroutine.html), [callback](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/executor_binder.html) and [Boost.Coroutine](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/basic_yield_context.html). There is also a special token created by `agrpc::use_sender(scheduler)` that causes RPC functions to return a [TypedSender](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender).

If you are interested in learning more about the implementation details of this library then check out [this blog article](https://medium.com/3yourmind/c-20-coroutines-for-asynchronous-grpc-services-5b3dab1d1d61).

<details><summary><b>Getting started</b></summary>
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
<sup><a href='/example/snippets/server.cpp#L306-L309' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/example/snippets/client.cpp#L246-L248' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` and run it. Make sure to shutdown the `server` before destructing the `grpc_context`. Also destruct the `grpc_context` before destructing the `server`. A `grpc_context` can only be run on one thread at a time.

<!-- snippet: run-grpc_context-server-side -->
<a id='snippet-run-grpc_context-server-side'></a>
```cpp
grpc_context.run();
server->Shutdown();
}  // grpc_context is destructed here before the server
```
<sup><a href='/example/snippets/server.cpp#L324-L328' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
auto guard = asio::make_work_guard(grpc_context);
```
<sup><a href='/example/snippets/client.cpp#L250-L252' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Where to go from here?

Check out the [examples](/example) and the [API documentation](https://tradias.github.io/asio-grpc/).

</p>
</details>


# What users are saying

> Asio-grpc abstracts away the implementation details of asynchronous grpc handling: crafting working code is easier, faster, less prone to errors and considerably more fun. At 3YOURMIND we reliably use asio-grpc in production since its very first release, allowing our developers to effortlessly implement low-latency/high-throughput asynchronous data transfer in time critical applications.

[@3YOURMIND](https://github.com/3YOURMIND)

> Our project is a real-time distributed motion capture system that uses your framework to stream data back and forward between multiple machines. Previously I have tried to build a bidirectional streaming framework from scratch using only gRPC. However, it's not maintainable and error-prone due to a large amount of service and streaming code. As a developer whose experienced both raw grpc and asio-grpc, I can tell that your framework is a real a game-changer for writing grpc code in C++. It has made my life much easier. I really appreciate the effort you have put into this project and your superior skills in designing c++ template code.

[@khanhha](https://github.com/khanhha)
