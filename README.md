# asio-grpc

[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=Tradias_asio-grpc&metric=coverage)](https://sonarcloud.io/dashboard?id=Tradias_asio-grpc) [![vcpkg](https://repology.org/badge/version-for-repo/vcpkg/asio-grpc.svg?header=vcpkg)](https://repology.org/project/asio-grpc/versions) [![conan](https://repology.org/badge/version-for-repo/conancenter/asio-grpc.svg?header=conan)](https://repology.org/project/asio-grpc/versions) [![hunter](https://img.shields.io/badge/hunter-asio_grpc-green.svg)](https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html)

An [Executor, Networking TS](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [std::execution](http://wg21.link/p2300) interface to [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html) for writing asynchronous [gRPC](https://grpc.io/) clients and servers using C++20 coroutines, Boost.Coroutines, Asio's stackless coroutines, callbacks, sender/receiver and more.

# Features

> In v3 the new [ClientRPC](https://tradias.github.io/asio-grpc/md_doc_client_rpc_cheat_sheet.html) and [ServerRPC](https://tradias.github.io/asio-grpc/md_doc_server_rpc_cheat_sheet.html) APIs will replace the current free-function API. Your feedback is highly appreciated. Please communicate via issues.

* Asio [ExecutionContext](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/ExecutionContext.html) compatible wrapper around [grpc::CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html)
* Support for all RPC types: unary, client-streaming, server-streaming and bidirectional-streaming with any mix of Asio [CompletionToken](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers) as well as [Sender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#sender-concept), including allocator customization
* Support for asynchronously waiting for [grpc::Alarms](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html) including cancellation through [cancellation_slots](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/cancellation_slot.html) and [StopTokens](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#stoptoken-concept)
* Support for `std::execution` through either [libunifex](https://github.com/facebookexperimental/libunifex) or [stdexec](https://github.com/NVIDIA/stdexec)
* Support for generic gRPC clients and servers
* No extra codegen required, works with the vanilla gRPC C++ plugin (`grpc_cpp_plugin`)
* No-Boost version with [standalone Asio](https://github.com/chriskohlhoff/asio)
* No-Asio version with [libunifex](https://github.com/facebookexperimental/libunifex) or [stdexec](https://github.com/NVIDIA/stdexec)
* CMake function to easily generate gRPC source files: [asio_grpc_protobuf_generate](/cmake/AsioGrpcProtobufGenerator.cmake)

# Requirements

Asio-grpc is a C++17, header-only library. To install it, CMake (3.14+) is all that is needed.

To use it, [gRPC](https://grpc.io/) and either [Boost.Asio](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio.html) (min. 1.74.0), [standalone Asio](https://github.com/chriskohlhoff/asio) (min. 1.17.0), [libunifex](https://github.com/facebookexperimental/libunifex) or [stdexec](https://github.com/NVIDIA/stdexec) must be present and linked into your application.

Officially supported compilers are GCC 8+, Clang 10+, AppleClang 14+ and latest MSVC.

# Usage

The library can be added to a CMake project using either `add_subdirectory` or `find_package`. Once set up, include the individual headers from the agrpc/ directory or the convenience header:

```cpp
#include <agrpc/asio_grpc.hpp>
```

<details><summary><b>As a CMake subdirectory</b></summary>
<p>

Clone the repository into a subdirectory of your CMake project. Then add it and link it to your target.

Using [Boost.Asio](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio.html):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)

# Also link with the equivalents of gRPC::grpc++ and Boost::headers
```

Or using [standalone Asio](https://github.com/chriskohlhoff/asio):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-standalone-asio)

# Also link with the equivalents of gRPC::grpc++ and asio::asio
```

Or using [libunifex](https://github.com/facebookexperimental/libunifex):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-unifex)

# Also link with the equivalents of gRPC::grpc++ and unifex::unifex
```

Or using [stdexec](https://github.com/NVIDIA/stdexec):

```cmake
add_subdirectory(/path/to/asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-stdexec)

# Also link with the equivalents of gRPC::grpc++ and STDEXEC::stdexec
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

Using [Boost.Asio](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio.html):

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

Or using [stdexec](https://github.com/NVIDIA/stdexec):

```cmake
# Make sure CMAKE_PREFIX_PATH contains /desired/installation/directory
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-stdexec)
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
        // "libunifex",
        // To use the stdexec backend add
        // "stdexec"
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
# Or use the stdexec backend
#target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc-stdexec)
```

</p>
</details>

<details><summary><b>Using Hunter</b></summary>
<p>

See asio-grpc's documentation on the Hunter website: [https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html](https://hunter.readthedocs.io/en/latest/packages/pkg/asio-grpc.html).

</p>
</details>

<details><summary><b>Using conan</b></summary>
<p>

The recipe in conan-center is called [asio-grpc](https://conan.io/center/recipes/asio-grpc).   
If you are using conan's CMake generator then link with `asio-grpc::asio-grpc` independent of the backend that you choose:

```cmake
find_package(asio-grpc)
target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc)
```

### Available options

`backend` - One of "boost" for Boost.Asio, "asio" for standalone Asio or "unifex" for libunifex.

</p>
</details>

<details><summary><b>Raw source code</b></summary>
<p>

This type of usage is unsupported. Future versions of asio-grpc might break it without notice.

Copy the contents of the `src/` directory into your project and add it to your project's include directories. Depending on your desired backend: Boost.Asio, 
standalone Asio, libunifex or stdexec, set the preprocessor definitions `AGRPC_BOOST_ASIO`, `AGRPC_STANDALONE_ASIO`, `AGRPC_UNIFEX` or `AGRPC_STDEXEC` respectively. Also make sure that 
the backend's header files and libraries can be found correctly.

</p>
</details>

## CMake Options

`ASIO_GRPC_DISABLE_AUTOLINK` - Set before using `find_package(asio-grpc)` to prevent `asio-grpcConfig.cmake` from finding and setting up interface link libraries like `gRPC::grpc++`.

# Performance

asio-grpc is part of [grpc_bench](https://github.com/Tradias/grpc_bench). Head over there to compare its performance against other libraries and languages.

Below are the results from the helloworld unary RPC for:   
Intel(R) Core(TM) i7-8750H CPU @ 2.20GHz   
Linux, GCC 12.2.0, Boost 1.80.0, gRPC 1.52.1, asio-grpc v2.5.0, jemalloc 5.2.1   
Request scenario: string_100B

<details><summary><b>Results</b></summary>
<p>

### 1 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| rust_thruster_mt            |   48796 |       20.19 ms |        9.42 ms |       12.11 ms |      516.23 ms |  104.51% |     12.06 MiB |
| rust_tonic_mt               |   43343 |       22.86 ms |       10.42 ms |       11.29 ms |      662.73 ms |  102.29% |     14.39 MiB |
| go_grpc                     |   38541 |       25.33 ms |       38.74 ms |       42.98 ms |       53.94 ms |   100.0% |     25.19 MiB |
| rust_grpcio                 |   34757 |       28.65 ms |       30.18 ms |       30.60 ms |       31.68 ms |  101.91% |     18.59 MiB |
| cpp_grpc_mt                 |   33433 |       29.77 ms |       31.56 ms |       32.07 ms |       33.58 ms |  102.22% |      5.69 MiB |
| cpp_asio_grpc_callback      |   32521 |       30.61 ms |       32.54 ms |       33.14 ms |       35.29 ms |  101.65% |      5.93 MiB |
| cpp_asio_grpc_unifex        |   32507 |       30.62 ms |       32.50 ms |       32.99 ms |       34.66 ms |  102.94% |      5.81 MiB |
| cpp_asio_grpc_coroutine     |   28893 |       34.47 ms |       36.78 ms |       37.37 ms |       38.88 ms |  102.52% |      5.56 MiB |
| cpp_asio_grpc_io_context_coro |   28072 |       35.47 ms |       37.77 ms |       38.22 ms |       39.93 ms |   77.73% |      5.39 MiB |
| cpp_grpc_callback           |   10243 |       90.44 ms |      118.77 ms |      164.20 ms |      175.43 ms |  100.62% |      44.9 MiB |

### 2 CPU server

| name                        |   req/s |   avg. latency |        90 % in |        95 % in |        99 % in | avg. cpu |   avg. memory |
|-----------------------------|--------:|---------------:|---------------:|---------------:|---------------:|---------:|--------------:|
| cpp_grpc_mt                 |   87550 |        9.66 ms |       15.11 ms |       18.23 ms |       27.03 ms |  204.66% |     26.15 MiB |
| cpp_asio_grpc_unifex        |   86568 |        9.83 ms |       15.34 ms |       18.55 ms |       27.12 ms |  207.78% |     27.54 MiB |
| cpp_asio_grpc_callback      |   85292 |       10.03 ms |       15.38 ms |       18.51 ms |       26.62 ms |  206.63% |     24.73 MiB |
| cpp_asio_grpc_coroutine     |   79647 |       11.04 ms |       18.01 ms |       21.08 ms |       28.67 ms |  212.19% |     25.04 MiB |
| cpp_asio_grpc_io_context_coro |   77953 |       11.24 ms |       18.32 ms |       21.61 ms |       29.20 ms |  161.24% |      28.4 MiB |
| rust_thruster_mt            |   75793 |       11.90 ms |       26.84 ms |       40.49 ms |       59.71 ms |  186.64% |     13.85 MiB |
| cpp_grpc_callback           |   68203 |       12.24 ms |       23.93 ms |       28.62 ms |       41.83 ms |  206.38% |     52.79 MiB |
| rust_tonic_mt               |   67162 |       13.85 ms |       34.05 ms |       46.31 ms |       69.58 ms |  206.13% |     17.24 MiB |
| rust_grpcio                 |   60775 |       15.49 ms |       22.85 ms |       25.77 ms |       31.14 ms |  218.05% |     30.15 MiB |
| go_grpc                     |   58192 |       15.87 ms |       24.31 ms |       27.10 ms |       32.43 ms |  197.71% |     25.06 MiB |

</p>
</details>

# Documentation

[**Documentation**](https://tradias.github.io/asio-grpc/)
[**Examples**](/example)

The main workhorses of this library are the [agrpc::GrpcContext](https://tradias.github.io/asio-grpc/classagrpc_1_1_grpc_context.html) and its `executor_type` - [agrpc::GrpcExecutor](https://tradias.github.io/asio-grpc/classagrpc_1_1_basic_grpc_executor.html). 

The [agrpc::GrpcContext](https://tradias.github.io/asio-grpc/classagrpc_1_1_grpc_context.html) implements [asio::execution_context](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/execution_context.html) and can be used as an argument to Asio functions that expect an `ExecutionContext` like [asio::spawn](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/spawn/overload2.html).

Likewise, the [agrpc::GrpcExecutor](https://tradias.github.io/asio-grpc/classagrpc_1_1_basic_grpc_executor.html) satisfies the [Executor and Networking TS](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors) and [Scheduler](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#scheduler) requirements and can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.

The API for RPCs is modeled after the asynchronous, tag-based API of gRPC. As an example, the equivalent for `grpc::ClientAsyncReader<helloworld::HelloReply>.Read(helloworld::HelloReply*, void*)` would be `agrpc::ClientRPC.read(helloworld::HelloReply&, CompletionToken)`.

Instead of the `void*` tag in the gRPC API the functions in this library expect a [CompletionToken](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). Asio comes with several CompletionTokens already: [C++20 coroutine](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/use_awaitable.html), [stackless coroutine](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/coroutine.html), callback and [Boost.Coroutine](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/basic_yield_context.html). There is also a special token called `agrpc::use_sender` that causes RPC functions to return a [Sender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#sender-concept).

If you are interested in learning more about the implementation details of this library then check out [this blog article](https://medium.com/3yourmind/c-20-coroutines-for-asynchronous-grpc-services-5b3dab1d1d61).

Even more examples can be found in another [repository](https://github.com/Tradias/example-vcpkg-grpc#branches).
