# Copyright 2021 Dennis Hezel
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

# common compile options
add_library(asio-grpc-common-compile-options INTERFACE)

target_compile_options(
    asio-grpc-common-compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:
              /external:I
              $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
              /external:W1
              /external:templates-
              /W4>
              $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall
              -Wextra
              -pedantic-errors>)

target_compile_definitions(
    asio-grpc-common-compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:
              BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
              BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT
              _WIN32_WINNT=0x0A00 # Windows 10
              WINVER=0x0A00>
              BOOST_ASIO_NO_DEPRECATED)

target_link_libraries(asio-grpc-common-compile-options INTERFACE asio-grpc Boost::disable_autolinking)

# C++20 compile options
add_library(asio-grpc-cpp20-compile-options INTERFACE)

target_compile_features(asio-grpc-cpp20-compile-options INTERFACE cxx_std_20)

target_compile_options(asio-grpc-cpp20-compile-options INTERFACE $<$<CXX_COMPILER_ID:GNU>:-fcoroutines>)
