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

# common compile options
add_library(asio-grpc-compile-options INTERFACE)

if(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS)
    target_compile_options(
        asio-grpc-compile-options INTERFACE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fsanitize=undefined,leak
                                            -fno-omit-frame-pointer> $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)

    target_link_options(asio-grpc-compile-options INTERFACE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fsanitize=undefined,leak>)

    target_compile_definitions(asio-grpc-compile-options INTERFACE GRPC_ASAN_SUPPRESSED GRPC_TSAN_SUPPRESSED)
endif()

target_compile_options(
    asio-grpc-compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/W4
              /wd4996
              /permissive-
              /Zc:preprocessor
              /Zc:__cplusplus
              /Zc:inline
              /Zc:externConstexpr
              /Zc:lambda
              /Zc:sizedDealloc
              /Zc:throwingNew>
              $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall
              -Wextra
              -pedantic-errors
              -Wno-deprecated-declarations>
              $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-self-move>)

if("${CMAKE_GENERATOR}" STRGREATER_EQUAL "Visual Studio")
    target_compile_options(asio-grpc-compile-options INTERFACE /MP)
endif()

target_compile_definitions(
    asio-grpc-compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0A00> # Windows 10
              $<$<CXX_COMPILER_ID:Clang>:BOOST_ASIO_HAS_STD_INVOKE_RESULT ASIO_HAS_STD_INVOKE_RESULT>
              BOOST_ASIO_NO_DEPRECATED ASIO_NO_DEPRECATED)

target_link_libraries(asio-grpc-compile-options INTERFACE gRPC::grpc++_unsecure Boost::disable_autolinking)

target_compile_features(asio-grpc-compile-options INTERFACE cxx_std_17)

target_sources(asio-grpc-compile-options
               INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:${ASIO_GRPC_PROJECT_ROOT}/asio-grpc.natvis>")

if(ASIO_GRPC_USE_BOOST_CONTAINER)
    target_link_libraries(asio-grpc-compile-options INTERFACE Boost::container)
endif()

# C++20 compile options
add_library(asio-grpc-compile-options-cpp20 INTERFACE)

target_link_libraries(asio-grpc-compile-options-cpp20 INTERFACE asio-grpc-compile-options)

target_compile_features(asio-grpc-compile-options-cpp20 INTERFACE cxx_std_20)

target_compile_options(
    asio-grpc-compile-options-cpp20
    INTERFACE
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,10>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,12>>:-fcoroutines>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,11>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,14>>:-fcoroutines-ts>
)

# helper functions
function(convert_to_cpp_suffix _asio_grpc_cxx_standard)
    if(${_asio_grpc_cxx_standard} STREQUAL "20")
        set(_asio_grpc_cxx_standard
            "-cpp20"
            PARENT_SCOPE)
    else()
        set(_asio_grpc_cxx_standard PARENT_SCOPE)
    endif()
endfunction()
