# Contributing to asio-grpc

**Thank you for considering to contribute to this project!**

If you encountered a bug or want to make a feature request then do not hesitate to create a new issue. I am happy to look into it. 

If you intend to work on the code then please follow the steps below.

## Prerequisites

You will need a C++ compiler, CMake (3.19+) and a way of installing this project's dependencies: gRPC, Boost, standalone Asio, libunifex and doctest. I recommend [vcpkg](https://github.com/microsoft/vcpkg). 
Head over to their github repository for more details, but in general the following should get you started: 

```sh
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.bat  # or bootstrap-vcpkg.sh on Linux
```

## Build and run tests

From the root of the repository run:

```sh
# Make sure the VCPKG_ROOT env variable points to the to the cloned vcpkg repository
cmake --preset default
```

It might take a while until vcpkg has installed all dependencies.

Compile the tests and examples with:

```sh
cmake --build --preset default
```

And run all tests:

```sh
ctest --preset default
```

## Install git hooks

Before making a commit, install [clang-format](https://github.com/llvm/llvm-project/releases) (part of clang-tools-extra) and [cmake-format](https://pypi.org/project/cmake-format/). 
Re-run the CMake configure step and finally run:

```sh
cmake --build --preset default --target asio-grpc-init-git-hooks
```
