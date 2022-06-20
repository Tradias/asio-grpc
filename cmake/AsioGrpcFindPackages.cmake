# Copyright 2022 Dennis Hezel
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Suppress warnings, see https://cmake.org/cmake/help/v3.17/module/FindPackageHandleStandardArgs.html
set(FPHSA_NAME_MISMATCHED on)
find_package(protobuf)
unset(FPHSA_NAME_MISMATCHED)

find_package(gRPC)
if(ASIO_GRPC_USE_BOOST_CONTAINER)
    find_package(Boost REQUIRED COMPONENTS coroutine thread filesystem container)
else()
    find_package(Boost REQUIRED COMPONENTS coroutine thread filesystem)
endif()
find_package(asio)

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    find_package(unifex)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        find_package(PkgConfig)
        pkg_check_modules(liburing IMPORTED_TARGET GLOBAL liburing)
    endif()
endif()

# Fallback to pkg-config
if(NOT ASIO_GRPC_ENABLE_PKGCONFIG_FALLBACK)
    return()
endif()

find_package(PkgConfig)

if(NOT TARGET protobuf::libprotobuf)
    pkg_check_modules(protobuf REQUIRED IMPORTED_TARGET protobuf)
    add_library(protobuf::libprotobuf ALIAS PkgConfig::protobuf)
endif()

if(NOT TARGET protobuf::protoc)
    add_executable(protobuf::protoc IMPORTED)
    find_program(_asio_grpc_protoc_root protoc)
    set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "${_asio_grpc_protoc_root}")
endif()

if(NOT TARGET gRPC::grpc++)
    pkg_check_modules(grpc++ REQUIRED IMPORTED_TARGET grpc++)
    pkg_check_modules(grpc REQUIRED IMPORTED_TARGET grpc)
    add_library(gRPC INTERFACE IMPORTED)
    add_library(gRPC::grpc++ ALIAS gRPC)
    set_target_properties(gRPC PROPERTIES INTERFACE_LINK_LIBRARIES "PkgConfig::grpc++;PkgConfig::grpc")
endif()

if(NOT TARGET gRPC::grpc_cpp_plugin)
    add_executable(gRPC::grpc_cpp_plugin IMPORTED)
    find_program(_asio_grpc_grpc_cpp_plugin_root grpc_cpp_plugin)
    set_target_properties(gRPC::grpc_cpp_plugin PROPERTIES IMPORTED_LOCATION "${_asio_grpc_grpc_cpp_plugin_root}")
endif()

if(NOT TARGET asio::asio)
    find_path(_asio_grpc_asio_root "asio.hpp" REQUIRED)
    add_library(asio INTERFACE IMPORTED)
    add_library(asio::asio ALIAS asio)
    target_include_directories(asio INTERFACE "${_asio_grpc_asio_root}")
endif()

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    if(NOT TARGET unifex::unifex)
        pkg_check_modules(unifex REQUIRED IMPORTED_TARGET unifex)
        add_library(unifex::unifex ALIAS PkgConfig::unifex)
    endif()
endif()
