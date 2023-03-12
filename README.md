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

Supported compilers are GCC 8+, Clang 10+, AppleClang 14+ and latest MSVC.

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

`boost-container` (deprecated) - Use Boost.Container instead of `<memory_resource>`.

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

Please refer to the conan documentation on how to [use packages](https://docs.conan.io/en/latest/using_packages.html). The recipe in conan-center is called [asio-grpc/2.4.0](https://conan.io/center/asio-grpc).   
If you are using conan's CMake generator then link with `asio-grpc::asio-grpc` independent of the backend that you choose:

```cmake
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

### Available options

`backend` - One of "boost" for Boost.Asio, "asio" for standalone Asio or "unifex" for libunifex.

`local_allocator` (deprecated) - One of "memory_resource" for `<memory_resource>`, "boost_container" for Boost.Container, "recycling_allocator" for [asio::recycling_allocator](https://think-async.com/Asio/asio-1.24.0/doc/asio/reference/recycling_allocator.html).

</p>
</details>

## CMake Options

`ASIO_GRPC_USE_BOOST_CONTAINER` (deprecated) - Use Boost.Container instead of `<memory_resource>`. Mutually exclusive with `ASIO_GRPC_USE_RECYCLING_ALLOCATOR`.

`ASIO_GRPC_USE_RECYCLING_ALLOCATOR` (deprecated) - Use [asio::recycling_allocator](https://think-async.com/Asio/asio-1.24.0/doc/asio/reference/recycling_allocator.html) instead of `<memory_resource>`. Mutually exclusive with `ASIO_GRPC_USE_BOOST_CONTAINER`.

`ASIO_GRPC_DISABLE_AUTOLINK` - Set before using `find_package(asio-grpc)` to prevent `asio-grpcConfig.cmake` from finding and setting up interface link libraries like `gRPC::grpc++`.

# Performance

asio-grpc is part of [grpc_bench](https://github.com/Tradias/grpc_bench). Head over there to compare its performance against other libraries and languages.

Below are the results from the helloworld unary RPC for:   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz   
Linux, GCC 12.2.0, Boost 1.80.0, gRPC 1.50.0, asio-grpc v2.4.0, jemalloc 5.2.1   
Request scenario: string_100B

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_thruster_mt            |   48754 |       20.22 ms |        9.38 ms |       11.23 ms |      513.03 ms |  103.97% |     11.94 MiB |
| rust_tonic_mt               |   42770 |       23.17 ms |       10.34 ms |       11.10 ms |      669.23 ms |  102.71% |     14.43 MiB |
| go_grpc                     |   38472 |       25.39 ms |       38.61 ms |       43.03 ms |       54.13 ms |   98.99% |     25.28 MiB |
| rust_grpcio                 |   35048 |       28.41 ms |       29.84 ms |       30.30 ms |       32.65 ms |  102.94% |     17.74 MiB |
| cpp_grpc_mt                 |   33371 |       29.82 ms |       31.66 ms |       32.58 ms |       34.83 ms |  102.76% |      5.76 MiB |
| cpp_asio_grpc_unifex        |   32721 |       30.43 ms |       32.25 ms |       32.86 ms |       35.44 ms |  102.44% |      5.46 MiB |
| cpp_asio_grpc_callback      |   32492 |       30.64 ms |       32.45 ms |       33.12 ms |       35.11 ms |  103.82% |      5.48 MiB |
| cpp_asio_grpc_coroutine     |   29089 |       34.23 ms |       36.27 ms |       37.10 ms |       38.94 ms |  101.86% |      5.53 MiB |
| cpp_asio_grpc_io_context_coro |   28157 |       35.37 ms |       37.57 ms |       38.51 ms |       41.34 ms |    78.5% |      5.36 MiB |
| cpp_grpc_callback           |   10142 |       90.92 ms |      123.49 ms |      165.29 ms |      176.71 ms |  102.57% |     43.26 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_grpc_mt                 |   87665 |        9.71 ms |       15.19 ms |       18.19 ms |       26.77 ms |  206.15% |     25.01 MiB |
| cpp_asio_grpc_unifex        |   87236 |        9.76 ms |       15.41 ms |       18.46 ms |       26.22 ms |  205.13% |      26.0 MiB |
| cpp_asio_grpc_callback      |   85191 |       10.02 ms |       15.35 ms |       18.37 ms |       26.49 ms |  206.83% |     22.68 MiB |
| cpp_asio_grpc_coroutine     |   79233 |       11.10 ms |       18.17 ms |       21.42 ms |       28.67 ms |   206.0% |     25.27 MiB |
| cpp_asio_grpc_io_context_coro |   76520 |       11.53 ms |       19.07 ms |       22.44 ms |       29.86 ms |  158.35% |     27.67 MiB |
| rust_thruster_mt            |   76448 |       11.76 ms |       26.13 ms |       37.99 ms |       58.98 ms |  189.37% |     14.71 MiB |
| rust_tonic_mt               |   67477 |       13.80 ms |       33.91 ms |       46.42 ms |       69.08 ms |  210.09% |     16.88 MiB |
| cpp_grpc_callback           |   65782 |       12.65 ms |       25.92 ms |       31.98 ms |       47.67 ms |   203.9% |     58.74 MiB |
| rust_grpcio                 |   61248 |       15.38 ms |       23.20 ms |       25.78 ms |       31.02 ms |   217.6% |     28.53 MiB |
| go_grpc                     |   58734 |       15.71 ms |       24.00 ms |       26.84 ms |       32.40 ms |  198.43% |     26.55 MiB |

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
<sup><a href='/example/snippets/server.cpp#L355-L358' title='Snippet source file'>snippet source</a> | <a href='#snippet-create-grpc_context-server-side' title='Start of snippet'>anchor</a></sup>
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
