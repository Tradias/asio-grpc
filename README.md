# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=coverage)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![vcpkg](https://repology.org/badge/version-for-repo/vcpkg/asio-grpc.svg?header=vcpkg)](https://repology.org/project/asio-grpc/versions) [![conan](https://repology.org/badge/version-for-repo/conancenter/asio-grpc.svg?header=conan)](https://repology.org/project/asio-grpc/versions) [![hunter](https://img.shields.io/badge/hunter-asio_grpc-green.svg)](https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html)

An [Executor, Networking TS](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [std::execution](http://wg21.link/p2300) interface to [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html) for writing asynchronous [gRPC](https://grpc.io/) clients and servers using C++20 coroutines, Boost.Coroutines, Asio's stackless coroutines, callbacks, sender/receiver and more.

# Features

* Asio [ExecutionContext](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/ExecutionContext.html) compatible wrapper around [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html)
* [Executor and Networking TS](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) requirements fulfilling associated executor
* Support for all RPC types: unary, client-streaming, server-streaming and bidirectional-streaming with any mix of Asio [CompletionToken](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers) as well as [TypedSender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept), including allocator customization
* Support for asynchronously waiting for [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html)s including cancellation through [cancellation_slot](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/cancellation_slot.html)s and [StopToken](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#stoptoken-concept)s
* Initial support for `std::execution` concepts through [libunifex](https://github.com/facebookexperimental/libunifex) and Asio: [schedule](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/execution__schedule.html), [connect](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/execution__connect.html), [submit](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/execution__submit.html), [scheduler](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/Scheduler.html), [typed_sender](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/Sender.html#boost_asio.reference.Sender.typed_sender) and more
* Support for generic gRPC clients and servers (aka. proxies)
* No extra codegen required, works with the vanilla gRPC C++ plugin (`grpc_cpp_plugin`)
* Experimental support for Rust/Golang [select](https://go.dev/ref/spec#Select_statements)-style programming with the help of [cancellation safety](https://tradias.github.io/asio-grpc/classagrpc_1_1_basic_grpc_stream.html)
* No-Boost version with [standalone Asio](https://github.com/chriskohlhoff/asio)
* No-Asio version with [libunifex](https://github.com/facebookexperimental/libunifex)
* CMake function to generate gRPC source files: [asio_grpc_protobuf_generate](/cmake/AsioGrpcProtobufGenerator.cmake)

# Example

* Client-side 'hello world':

<!-- snippet: client-side-helloworld -->
<a id='snippet-client-side-helloworld'></a>
```cpp
helloworld::Greeter::Stub stub{grpc::CreateChannel(host, grpc::InsecureChannelCredentials())};
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

asio::co_spawn(
    grpc_context,
    [&]() -> asio::awaitable<void>
    {
        using RPC = agrpc::RPC<&helloworld::Greeter::Stub::PrepareAsyncSayHello>;
        grpc::ClientContext client_context;
        helloworld::HelloRequest request;
        request.set_name("world");
        helloworld::HelloReply response;
        status = co_await RPC::request(grpc_context, stub, client_context, request, response, asio::use_awaitable);
        std::cout << status.ok() << " response: " << response.message() << std::endl;
    },
    asio::detached);

grpc_context.run();
```
<sup><a href='/example/hello-world-client.cpp#L33-L52' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

More examples for things like streaming RPCs, double-buffered file transfer with io_uring, libunifex-based coroutines, sharing a thread with an io_context and generic clients/servers can be found in the [example](/example) directory. Even more examples can be found in another [repository](https://github.com/Tradias/example-vcpkg-grpc#branches).

# Requirements

Asio-grpc requires [gRPC](https://grpc.io/) and either [Boost.Asio](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio.html) (min. 1.74.0), [standalone Asio](https://github.com/chriskohlhoff/asio) (min. 1.17.0) or [libunifex](https://github.com/facebookexperimental/libunifex).

CMake version 3.14 is required to install project, but the `src/` directory may also be copied and used directly.

Versions tested by Github Actions:

 * CMake 3.16.3
 * gRPC 1.46.3, 1.16.1 (older versions might work as well)
 * Boost 1.80.0
 * Standalone Asio 1.17.0
 * libunifex 2022-02-09
 * MSVC 19.33 (Visual Studio 17 2022)
 * GCC 8.4.0, 10.3.0, 11.1.0
 * Clang 10.0.0, 12.0.0
 * AppleClang 14
 * C++17 and C++20

# Usage

The library can be added to a CMake project using either `add_subdirectory` or `find_package`. Once set up, include the individual headers from the agrpc/ directory or the convenience header:

```cpp
#include <agrpc/asio_grpc.hpp>
```

<details><summary><b>As a subdirectory</b></summary>
<p>

Clone the repository into a subdirectory of your CMake project. Then add it and link it to your target.

Using [Boost.Asio](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio.html):

```cmake
find_package(gRPC)
find_package(Boost)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC gRPC::grpc++_unsecure asio-grpc::asio-grpc Boost::headers)
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
find_package(gRPC)
find_package(asio)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC gRPC::grpc++_unsecure asio-grpc::asio-grpc-standalone-asio asio::asio)
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
find_package(gRPC)
find_package(unifex)
add_subdirectory(/path/to/repository/root)
target_link_libraries(your_app PUBLIC gRPC::grpc++_unsecure asio-grpc::asio-grpc-unifex unifex::unifex)
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

Using [Boost.Asio](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio.html):

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

`boost-container` - Use Boost.Container instead of `<memory_resource>`.

See [selecting-library-features](https://vcpkg.io/en/docs/users/selecting-library-features.html) to learn how to select features with vcpkg.

</p>
</details>

<details><summary><b>Using Hunter</b></summary>
<p>

See asio-grpc's documentation on the Hunter website: [https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html](https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html).

</p>
</details>

<details><summary><b>Using conan</b></summary>
<p>

Please refer to the conan documentation on how to [use packages](https://docs.conan.io/en/latest/using_packages.html). The recipe in conan-center is called [asio-grpc/2.0.0](https://conan.io/center/asio-grpc).   
If you are using conan's CMake generator then link with `asio-grpc::asio-grpc` independent of the backend that you choose:

```cmake
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

### Available options

`backend` - One of "boost" for Boost.Asio, "asio" for standalone Asio or "unifex" for libunifex.

`use_boost_container` - "True" to use Boost.Container instead of `<memory_resource>`.

</p>
</details>

## CMake Options

`ASIO_GRPC_USE_BOOST_CONTAINER` - Use Boost.Container instead of `<memory_resource>`.

`ASIO_GRPC_DISABLE_AUTOLINK` - Set before using `find_package(asio-grpc)` to prevent `asio-grpcConfig.cmake` from finding and setting up interface link libraries.

# Performance

asio-grpc is part of [grpc_bench](https://github.com/Tradias/grpc_bench). Head over there to compare its performance against other libraries and languages.

Results from the helloworld unary RPC   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, GCC 12.1.0, Boost 1.79.0, gRPC 1.48.0, asio-grpc v2.0.0, jemalloc 5.2.1   
Request scenario: string_100B

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| go_grpc                     |   48414 |       19.93 ms |       30.18 ms |       33.47 ms |       39.95 ms |  100.86% |     25.06 MiB |
| rust_thruster_mt            |   45317 |       21.87 ms |        9.70 ms |       11.11 ms |      641.64 ms |  102.59% |     11.67 MiB |
| rust_tonic_mt               |   40461 |       24.54 ms |       10.77 ms |       11.74 ms |      655.83 ms |   102.5% |     13.12 MiB |
| cpp_grpc_mt                 |   35571 |       27.98 ms |       29.67 ms |       30.08 ms |       31.35 ms |  103.07% |       5.1 MiB |
| rust_grpcio                 |   35451 |       28.08 ms |       29.79 ms |       30.35 ms |       31.21 ms |  102.48% |     18.08 MiB |
| cpp_asio_grpc_unifex        |   33908 |       29.36 ms |       31.24 ms |       31.68 ms |       32.93 ms |  103.81% |      5.52 MiB |
| cpp_asio_grpc_callback      |   33155 |       30.03 ms |       31.93 ms |       32.43 ms |       34.03 ms |  103.35% |      6.46 MiB |
| cpp_asio_grpc_coroutine     |   31175 |       31.95 ms |       34.03 ms |       34.56 ms |       36.05 ms |  101.82% |      5.07 MiB |
| cpp_asio_grpc_io_context_coro |   29658 |       33.58 ms |       35.93 ms |       36.38 ms |       37.71 ms |   77.96% |      5.62 MiB |
| cpp_grpc_callback           |   10057 |       91.56 ms |      137.04 ms |      166.93 ms |      179.02 ms |  101.51% |     47.08 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_asio_grpc_unifex        |   81705 |       10.40 ms |       15.91 ms |       18.98 ms |       27.27 ms |  210.83% |     26.59 MiB |
| cpp_grpc_mt                 |   81053 |       10.41 ms |       16.32 ms |       19.70 ms |       28.66 ms |  206.76% |     26.28 MiB |
| cpp_asio_grpc_callback      |   79425 |       10.73 ms |       16.54 ms |       19.81 ms |       28.51 ms |  208.81% |      23.9 MiB |
| cpp_asio_grpc_coroutine     |   73646 |       11.81 ms |       19.23 ms |       22.44 ms |       30.80 ms |  210.83% |     24.79 MiB |
| cpp_asio_grpc_io_context_coro |   70568 |       12.41 ms |       20.43 ms |       24.03 ms |       33.61 ms |  160.64% |     24.76 MiB |
| go_grpc                     |   65692 |       13.24 ms |       20.63 ms |       23.63 ms |       30.85 ms |  194.68% |     25.44 MiB |
| rust_thruster_mt            |   65361 |       13.87 ms |       36.33 ms |       59.45 ms |       81.96 ms |  193.87% |      13.4 MiB |
| cpp_grpc_callback           |   64623 |       12.64 ms |       24.25 ms |       29.55 ms |       42.64 ms |  206.96% |     55.59 MiB |
| rust_tonic_mt               |   58563 |       16.01 ms |       41.97 ms |       63.81 ms |       98.95 ms |  201.64% |     15.45 MiB |
| rust_grpcio                 |   57612 |       16.40 ms |       24.81 ms |       27.37 ms |       32.15 ms |  215.98% |     29.94 MiB |

</p>
</details>

# Documentation

[**Documentation**](https://tradias.github.io/asio-grpc/)

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [asio::execution_context](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Asio functions that expect an `ExecutionContext` like [asio::spawn](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/spawn/overload2.html).

Likewise, the `agrpc::GrpcExecutor` satisfies the [Executor and Networking TS](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Scheduler](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/Scheduler.html) requirements and can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.

The API for RPCs is modeled closely after the asynchronous, tag-based API of gRPC. As an example, the equivalent for `grpc::ClientAsyncReader<helloworld::HelloReply>.Read(helloworld::HelloReply*, void*)` would be `agrpc::read(grpc::ClientAsyncReader<helloworld::HelloReply>&, helloworld::HelloReply&, CompletionToken)`.

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/use_awaitable.html), [stackless coroutine](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/coroutine.html), [callback](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/executor_binder.html) and [Boost.Coroutine](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/basic_yield_context.html). There is also a special token called `agrpc::use_sender` that causes RPC functions to return a [TypedSender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept).

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
<sup><a href='/example/snippets/server.cpp#L352-L355' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
```
<sup><a href='/example/snippets/client.cpp#L352-L354' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` and run it. As an example, a simple unary request using [asio::use_awaitable](https://www.boost.org/doc/libs/1_80_0/doc/html/boost_asio/reference/use_awaitable.html) (the default completion token):

<!-- snippet: run-grpc_context-client-side -->
<a id='snippet-run-grpc_context-client-side'></a>
```cpp
example::v1::Example::Stub stub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
asio::co_spawn(
    grpc_context,
    [&]() -> asio::awaitable<void>
    {
        grpc::ClientContext client_context;
        example::v1::Request request;
        request.set_integer(42);
        example::v1::Response response;
        using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncUnary>;
        grpc::Status status = co_await RPC::request(grpc_context, stub, client_context, request, response);
        assert(status.ok());
    },
    asio::detached);
grpc_context.run();
```
<sup><a href='/example/snippets/client.cpp#L356-L372' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

It might also be helpful to create a work guard before running the `agrpc::GrpcContext` to prevent `grpc_context.run()` from returning early.

<!-- snippet: make-work-guard -->
<a id='snippet-make-work-guard'></a>
```cpp
std::optional guard{asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)};
```
<sup><a href='/example/snippets/client.cpp#L374-L376' title='Snippet source file'>snippet source</a> | <a href='#snippet-make-work-guard' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Where to go from here?

Check out the [examples](/example) and the [documentation](https://tradias.github.io/asio-grpc/).

</p>
</details>


# What users are saying

> Asio-grpc abstracts away the implementation details of asynchronous grpc handling: crafting working code is easier, faster, less prone to errors and considerably more fun. At 3YOURMIND we reliably use asio-grpc in production since its very first release, allowing our developers to effortlessly implement low-latency/high-throughput asynchronous data transfer in time critical applications.

[@3YOURMIND](https://github.com/3YOURMIND)

> Our project is a real-time distributed motion capture system that uses your framework to stream data back and forward between multiple machines. Previously I have tried to build a bidirectional streaming framework from scratch using only gRPC. However, it's not maintainable and error-prone due to a large amount of service and streaming code. As a developer whose experienced both raw grpc and asio-grpc, I can tell that your framework is a real a game-changer for writing grpc code in C++. It has made my life much easier. I really appreciate the effort you have put into this project and your superior skills in designing c++ template code.

[@khanhha](https://github.com/khanhha)
