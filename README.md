# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=coverage)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![vcpkg](https://repology.org/badge/version-for-repo/vcpkg/asio-grpc.svg?header=vcpkg)](https://repology.org/project/asio-grpc/versions) [![conan](https://repology.org/badge/version-for-repo/conancenter/asio-grpc.svg?header=conan)](https://repology.org/project/asio-grpc/versions) [![hunter](https://img.shields.io/badge/hunter-asio_grpc-green.svg)](https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html)

An [Executor, Networking TS](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [std::execution](http://wg21.link/p2300) interface to [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html) for writing asynchronous [gRPC](https://grpc.io/) clients and servers using C++20 coroutines, Boost.Coroutines, Asio's stackless coroutines, callbacks, sender/receiver and more.

# Features

* Asio [ExecutionContext](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/ExecutionContext.html) compatible wrapper around [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html)
* [Executor and Networking TS](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) requirements fulfilling associated executor
* Support for all RPC types: unary, client-streaming, server-streaming and bidirectional-streaming with any mix of Asio [CompletionToken](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers) as well as [TypedSender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept), including allocator customization
* Support for asynchronously waiting for [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html)s including cancellation through [cancellation_slot](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/cancellation_slot.html)s and [StopToken](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#stoptoken-concept)s
* Support for `std::execution` through [libunifex](https://github.com/facebookexperimental/libunifex)
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
agrpc::GrpcContext grpc_context;

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

Asio-grpc is a C++17, header-only library. To install it, CMake (3.14+) is all that is needed.

To use it, [gRPC](https://grpc.io/) and either [Boost.Asio](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio.html) (min. 1.74.0), [standalone Asio](https://github.com/chriskohlhoff/asio) (min. 1.17.0) or [libunifex](https://github.com/facebookexperimental/libunifex) must be present and linked into your application.

Versions tested by Github Actions:

 * CMake 3.16.3
 * gRPC 1.50.1, 1.16.1 (older versions might work as well)
 * Boost 1.81.0
 * Standalone Asio 1.17.0
 * libunifex 2022-10-10
 * MSVC 19.34 (Visual Studio 17 2022)
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

Using [Boost.Asio](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio.html):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)

# Also link with the equivalents of gRPC::grpc++_unsecure, Boost::headers and
# Boost::container (if ASIO_GRPC_USE_BOOST_CONTAINER has been set)
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio)

# Also link with the equivalents of gRPC::grpc++_unsecure, asio::asio and
# Boost::container (if ASIO_GRPC_USE_BOOST_CONTAINER has been set)
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-unifex)

# Also link with the equivalents of gRPC::grpc++_unsecure, unifex::unifex and
# Boost::container (if ASIO_GRPC_USE_BOOST_CONTAINER has been set)
```

Set [optional options](#cmake-options) before calling `add_subdirectory`. Example:

```cmake
set(ASIO_GRPC_USE_BOOST_CONTAINER on)
add_subdirectory(/path/to/asio-grpc)
```

</p>
</details>

<details><summary><b>As a CMake package</b></summary>
<p>

Clone the repository and install it. Append any [optional options](#cmake-options) like `-DASIO_GRPC_USE_BOOST_CONTAINER=on` to the cmake configure call.

```shell
cmake -B build -DCMAKE_INSTALL_PREFIX=/desired/installation/directory .
cmake --build build --target install
```

Locate it and link it to your target.

Using [Boost.Asio](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio.html):

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

Please refer to the conan documentation on how to [use packages](https://docs.conan.io/en/latest/using_packages.html). The recipe in conan-center is called [asio-grpc/2.3.0](https://conan.io/center/asio-grpc).   
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

`ASIO_GRPC_USE_BOOST_CONTAINER` - Use Boost.Container instead of `<memory_resource>`. Mutually exclusive with `ASIO_GRPC_USE_RECYCLING_ALLOCATOR`.

`ASIO_GRPC_USE_RECYCLING_ALLOCATOR` - Use [asio::recycling_allocator](https://think-async.com/Asio/asio-1.24.0/doc/asio/reference/recycling_allocator.html) instead of `<memory_resource>`. Mutually exclusive with `ASIO_GRPC_USE_BOOST_CONTAINER`.

`ASIO_GRPC_DISABLE_AUTOLINK` - Set before using `find_package(asio-grpc)` to prevent `asio-grpcConfig.cmake` from finding and setting up interface link libraries like `gRPC::grpc++`.

# Performance

asio-grpc is part of [grpc_bench](https://github.com/Tradias/grpc_bench). Head over there to compare its performance against other libraries and languages.

Results from the helloworld unary RPC   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, GCC 12.2.0, Boost 1.80.0, gRPC 1.50.0, asio-grpc v2.2.0, jemalloc 5.2.1   
Request scenario: string_100B

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_thruster_mt            |   51918 |       19.03 ms |        8.66 ms |       10.26 ms |      499.22 ms |  103.22% |     11.69 MiB |
| rust_tonic_mt               |   46671 |       21.24 ms |        9.61 ms |       10.35 ms |      604.04 ms |  101.72% |     14.04 MiB |
| go_grpc                     |   40699 |       23.97 ms |       36.68 ms |       40.75 ms |       50.16 ms |   98.72% |     25.28 MiB |
| rust_grpcio                 |   38222 |       26.06 ms |       27.90 ms |       28.39 ms |       29.43 ms |  102.23% |     17.89 MiB |
| cpp_grpc_mt                 |   36133 |       27.55 ms |       29.28 ms |       29.75 ms |       31.31 ms |   102.0% |       5.2 MiB |
| cpp_asio_grpc_callback      |   33796 |       29.47 ms |       31.47 ms |       31.89 ms |       33.01 ms |  102.74% |      5.39 MiB |
| cpp_asio_grpc_unifex        |   33781 |       29.48 ms |       31.36 ms |       31.78 ms |       33.41 ms |  103.35% |      5.33 MiB |
| cpp_asio_grpc_io_context_coro |   30706 |       32.44 ms |       34.68 ms |       35.14 ms |       36.44 ms |    77.5% |      5.69 MiB |
| cpp_asio_grpc_coroutine     |   30687 |       32.46 ms |       34.94 ms |       35.47 ms |       37.00 ms |  102.17% |      5.63 MiB |
| cpp_grpc_callback           |   10022 |       92.43 ms |      145.80 ms |      168.15 ms |      179.91 ms |  101.33% |     51.37 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_asio_grpc_unifex        |   89159 |        9.64 ms |       15.43 ms |       18.27 ms |       25.87 ms |  208.59% |     29.07 MiB |
| cpp_grpc_mt                 |   88992 |        9.60 ms |       15.18 ms |       18.22 ms |       25.32 ms |   208.7% |     23.78 MiB |
| cpp_asio_grpc_callback      |   88078 |        9.67 ms |       15.25 ms |       18.31 ms |       26.06 ms |  212.87% |     28.64 MiB |
| cpp_asio_grpc_coroutine     |   79508 |       10.83 ms |       18.09 ms |       21.45 ms |       28.30 ms |  211.46% |     29.47 MiB |
| rust_thruster_mt            |   76600 |       11.69 ms |       28.52 ms |       44.30 ms |       64.47 ms |  188.57% |     14.48 MiB |
| cpp_asio_grpc_io_context_coro |   75468 |       11.67 ms |       20.31 ms |       23.45 ms |       31.33 ms |  159.76% |     25.69 MiB |
| rust_tonic_mt               |   70692 |       13.11 ms |       32.60 ms |       44.63 ms |       69.52 ms |  205.38% |     17.48 MiB |
| cpp_grpc_callback           |   66659 |       12.40 ms |       25.75 ms |       30.73 ms |       45.06 ms |  205.51% |     62.18 MiB |
| rust_grpcio                 |   65566 |       14.32 ms |       21.37 ms |       23.68 ms |       28.48 ms |  215.38% |     32.33 MiB |
| go_grpc                     |   61824 |       14.87 ms |       22.80 ms |       25.62 ms |       30.65 ms |  200.74% |     26.94 MiB |

</p>
</details>

# Documentation

[**Documentation**](https://tradias.github.io/asio-grpc/)

The main workhorses of this library are the `agrpc::GrpcContext` and its `executor_type` - `agrpc::GrpcExecutor`. 

The `agrpc::GrpcContext` implements [asio::execution_context](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Asio functions that expect an `ExecutionContext` like [asio::spawn](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/spawn/overload2.html).

Likewise, the `agrpc::GrpcExecutor` satisfies the [Executor and Networking TS](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Scheduler](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#scheduler) requirements and can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.

The API for RPCs is modeled closely after the asynchronous, tag-based API of gRPC. As an example, the equivalent for `grpc::ClientAsyncReader<helloworld::HelloReply>.Read(helloworld::HelloReply*, void*)` would be `agrpc::read(grpc::ClientAsyncReader<helloworld::HelloReply>&, helloworld::HelloReply&, CompletionToken)`.

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/use_awaitable.html), [stackless coroutine](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/coroutine.html), [callback](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/executor_binder.html) and [Boost.Coroutine](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/basic_yield_context.html). There is also a special token called `agrpc::use_sender` that causes RPC functions to return a [TypedSender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept).

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
<sup><a href='/example/snippets/server.cpp#L393-L396' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

For clients only:

<!-- snippet: create-grpc_context-client-side -->
<a id='snippet-create-grpc_context-client-side'></a>
```cpp
agrpc::GrpcContext grpc_context;
```
<sup><a href='/example/snippets/client.cpp#L357-L359' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

Add some work to the `grpc_context` and run it. As an example, a simple unary request using [asio::use_awaitable](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/use_awaitable.html) (the default completion token):

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
<sup><a href='/example/snippets/client.cpp#L361-L377' title='Snippet source file'>snippet source</a> | <a href='#snippet-run-grpc_context-client-side' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Where to go from here?

Check out the [examples](/example) and the [documentation](https://tradias.github.io/asio-grpc/).

</p>
</details>
