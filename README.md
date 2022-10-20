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
 * gRPC 1.49.0, 1.16.1 (older versions might work as well)
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
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz, Linux, GCC 12.2.0, Boost 1.80.0, gRPC 1.50.0, asio-grpc v2.2.0, jemalloc 5.2.1   
Request scenario: string_100B

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_thruster_mt            |   49994 |       19.73 ms |        8.95 ms |       10.77 ms |      515.24 ms |   105.0% |     12.15 MiB |
| rust_tonic_mt               |   43996 |       22.53 ms |       10.17 ms |       10.90 ms |      658.49 ms |  103.34% |     14.65 MiB |
| go_grpc                     |   39365 |       24.71 ms |       37.52 ms |       41.89 ms |       51.94 ms |   99.48% |     24.83 MiB |
| rust_grpcio                 |   36516 |       27.27 ms |       29.24 ms |       29.75 ms |       30.84 ms |  103.41% |     18.36 MiB |
| cpp_grpc_mt                 |   34849 |       28.57 ms |       30.34 ms |       30.86 ms |       32.30 ms |  102.56% |      5.77 MiB |
| cpp_asio_grpc_unifex        |   34065 |       29.22 ms |       31.12 ms |       31.70 ms |       33.13 ms |  102.71% |      5.55 MiB |
| cpp_asio_grpc_callback      |   33322 |       29.88 ms |       31.73 ms |       32.15 ms |       33.17 ms |  103.08% |      5.47 MiB |
| cpp_asio_grpc_cpp20_coroutine |   32543 |       30.60 ms |       32.53 ms |       33.05 ms |       34.23 ms |  102.98% |      5.55 MiB |
| cpp_asio_grpc_io_context_coro |   29236 |       34.07 ms |       36.60 ms |       37.07 ms |       38.22 ms |   77.59% |      5.55 MiB |
| cpp_grpc_callback           |    9712 |       94.97 ms |      159.10 ms |      167.45 ms |      179.10 ms |  100.64% |     39.83 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_grpc_mt                 |   83032 |       10.14 ms |       15.84 ms |       18.80 ms |       26.93 ms |  204.45% |     26.31 MiB |
| cpp_asio_grpc_unifex        |   82886 |       10.27 ms |       16.13 ms |       19.21 ms |       28.12 ms |  207.27% |     28.87 MiB |
| cpp_asio_grpc_callback      |   81464 |       10.51 ms |       16.56 ms |       19.62 ms |       27.60 ms |  208.05% |     28.76 MiB |
| cpp_asio_grpc_cpp20_coroutine |   80176 |       10.68 ms |       16.83 ms |       19.83 ms |       27.57 ms |  205.33% |     30.79 MiB |
| rust_thruster_mt            |   72647 |       12.27 ms |       28.15 ms |       41.74 ms |       63.25 ms |   189.5% |     14.56 MiB |
| cpp_asio_grpc_io_context_coro |   71029 |       12.34 ms |       19.96 ms |       23.51 ms |       32.72 ms |  159.03% |     22.65 MiB |
| cpp_grpc_callback           |   66411 |       12.31 ms |       23.51 ms |       28.43 ms |       40.44 ms |  205.36% |     59.76 MiB |
| rust_tonic_mt               |   66068 |       14.04 ms |       34.90 ms |       47.45 ms |       71.99 ms |  207.32% |     17.56 MiB |
| rust_grpcio                 |   59409 |       15.90 ms |       23.30 ms |       25.64 ms |       30.70 ms |  216.12% |     31.65 MiB |
| go_grpc                     |   58654 |       15.65 ms |       24.06 ms |       26.83 ms |       32.26 ms |   197.8% |     25.15 MiB |

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
