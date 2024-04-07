// Copyright 2023 Dennis Hezel
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

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

// Unlikely
#ifndef AGRPC_UNLIKELY

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

#endif

// Likely
#ifndef AGRPC_LIKELY

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

#endif

// Try-catch
#ifndef AGRPC_TRY

#if __cpp_exceptions >= 199711L
#define AGRPC_TRY try
#define AGRPC_CATCH(...) catch (__VA_ARGS__)
#else
#define AGRPC_TRY
#define AGRPC_CATCH(...) \
    if constexpr (true)  \
    {                    \
    }                    \
    else
#endif

#endif

// Namespace
#ifndef AGRPC_NAMESPACE_BEGIN

#ifdef AGRPC_GENERATING_DOCUMENTATION
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {
#elif defined(AGRPC_STANDALONE_ASIO)
#if defined(AGRPC_UNIFEX)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace r          \
    {
#elif defined(AGRPC_STDEXEC)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace t          \
    {
#else
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace s          \
    {
#endif
#elif defined(AGRPC_BOOST_ASIO)
#if defined(AGRPC_UNIFEX)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace a          \
    {
#elif defined(AGRPC_STDEXEC)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace c          \
    {
#else
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace b          \
    {
#endif
#elif defined(AGRPC_UNIFEX)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace u          \
    {
#elif defined(AGRPC_STDEXEC)
#define AGRPC_NAMESPACE_BEGIN() \
    namespace agrpc             \
    {                           \
    inline namespace e          \
    {
#else
static_assert(false,
              "asio-grpc backend macro is not defined. Did you forget to link with `asio-grpc::asio-grpc`, "
              "`asio-grpc::asio-grpc-standalone-asio` or `asio-grpc::asio-grpc-unifex` in your CMake file?");
#endif

#ifdef AGRPC_GENERTING_DOCUMENTATION
#define AGRPC_NAMESPACE_END }
#else
#define AGRPC_NAMESPACE_END \
    }                       \
    }
#endif

#endif
