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

enable_language(CXX)

set(ASIO_GRPC_DEFAULT_USE_BOOST_CONTAINER off)

if(ASIO_GRPC_USE_RECYCLING_ALLOCATOR)
    return()
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"
   OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "IntelLLVM"
   OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "9"))
    set(ASIO_GRPC_DEFAULT_USE_BOOST_CONTAINER on)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    string(FIND "${CMAKE_CXX_FLAGS}" "-stdlib=libc++" ASIO_GRPC_CXX_FLAGS_CONTAIN_LIBCXX)
    if(NOT "${ASIO_GRPC_CXX_FLAGS_CONTAIN_LIBCXX}" STREQUAL "-1")
        set(ASIO_GRPC_DEFAULT_USE_BOOST_CONTAINER on)
    endif()
endif()
