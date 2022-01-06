// Copyright 2022 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AGRPC_DETAIL_ATTRIBUTES_HPP
#define AGRPC_DETAIL_ATTRIBUTES_HPP

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(unlikely) && ((defined(_MSVC_LANG) && _MSVC_LANG > 201703L) || __cplusplus > 201703L)
#if (defined(__GNUC__) && __GNUC__ > 9) || !defined(__GNUC__)
#define AGRPC_UNLIKELY(...) (__VA_ARGS__) [[unlikely]]
#endif
#endif
#endif

#ifndef AGRPC_UNLIKELY
#if defined(__clang__) || defined(__GNUC__)
#define AGRPC_UNLIKELY(...) (__builtin_expect(!!(__VA_ARGS__), 0))
#else
#define AGRPC_UNLIKELY
#endif
#endif

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(likely) && ((defined(_MSVC_LANG) && _MSVC_LANG > 201703L) || __cplusplus > 201703L)
#if (defined(__GNUC__) && __GNUC__ > 9) || !defined(__GNUC__)
#define AGRPC_LIKELY(...) (__VA_ARGS__) [[likely]]
#endif
#endif
#endif

#ifndef AGRPC_LIKELY
#if defined(__clang__) || defined(__GNUC__)
#define AGRPC_LIKELY(...) (__builtin_expect(!!(__VA_ARGS__), 1))
#else
#define AGRPC_LIKELY
#endif
#endif

#if __cpp_exceptions >= 199711
#define AGRPC_TRY try
#define AGRPC_CATCH(...) catch (__VA_ARGS__)
#define AGRPC_RETHROW() throw
#else
#define AGRPC_TRY
#define AGRPC_CATCH(...) \
    if constexpr (true)  \
    {                    \
    }                    \
    else
#define AGRPC_RETHROW() ((void)0)
#endif

#ifdef AGRPC_BOOST_ASIO
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace b          \
    {
#elif defined(AGRPC_STANDALONE_ASIO)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace s          \
    {
#elif defined(AGRPC_UNIFEX)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace u          \
    {
#else
static_assert(false,
              "asio-grpc backend macro is not defined. Did you forget to link with `asio-grpc::asio-grpc`, "
              "`asio-grpc::asio-grpc-standalone-asio` or `asio-grpc::asio-grpc-unifex` in your CMake file?");
#endif

#define AGRPC_NAMESPACE_END \
    }                       \
    }

#endif  // AGRPC_DETAIL_ATTRIBUTES_HPP
