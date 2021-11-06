# Contributing to asio-grpc

**Thank you for considering to contribute to this project!**

If you encountered a bug or want to make a feature request then do not hesitate to create a new issue. I am happy to look into it. 

If you intend on working on the code then please read on.

## Prerequisites

You will need a C++ compiler, CMake (3.19+) and a way of installing this project's dependencies: gRPC and Boost. I recommend [vcpkg](https://github.com/microsoft/vcpkg). 
Head over to their github repository for more details, but in general the following should get you started: 

```sh
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.bat  # or .sh on Linux
```

## Build and run tests

From the root of the repository run:

```sh
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
  -DASIO_GRPC_BUILD_TESTS=on \
  -DASIO_GRPC_DISCOVER_TESTS=on \
  -DASIO_GRPC_USE_BOOST_CONTAINER=on  # if you are using a C++ compiler without <memory_resource>
```

It might take a while until vcpkg has installed all dependencies.

Compile the tests and examples with:

```sh
cmake --build ./build --parallel
```

And run all tests:

```sh
ctest --test-dir ./build -T test --output-on-failure
```

## Install git hooks

Before making a commit install [clang-format](https://github.com/llvm/llvm-project/releases) (part of clang-tools-extra) and [cmake-format](https://pypi.org/project/cmake-format/). 
Re-run the CMake configure step and ensure that the CMake cache variables `ASIO_GRPC_CMAKE_FORMAT_PROGRAM` and `ASIO_GRPC_CLANG_FORMAT_PROGRAM` are set correctly. Finally run:

```sh
cmake --build ./build --target asio-grpc-init-git-hooks
```
