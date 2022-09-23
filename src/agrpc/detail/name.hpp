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

#ifndef AGRPC_DETAIL_NAME_HPP
#define AGRPC_DETAIL_NAME_HPP

#include <agrpc/detail/algorithm.hpp>
#include <agrpc/detail/config.hpp>

#include <string_view>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <size_t N>
struct StaticString
{
    char data_[N];

    constexpr operator std::string_view() const noexcept { return {data_, N}; }

    constexpr auto size() const noexcept { return N; }

    constexpr auto data() const noexcept { return data_; }

    constexpr auto begin() noexcept { return data_; }

    constexpr auto begin() const noexcept { return data_; }

    constexpr auto end() noexcept { return data_ + N; }

    constexpr auto end() const noexcept { return data_ + N; }
};

template <class... U>
StaticString(U...) -> StaticString<sizeof...(U)>;

struct StringView
{
    const char* data_;
    size_t size_;

    constexpr auto size() const noexcept { return size_; }

    constexpr auto begin() const noexcept { return data_; }

    constexpr auto end() const noexcept { return data_ + size_; }

    constexpr auto substr(size_t pos, size_t count) const noexcept { return StringView{data_ + pos, count}; }
};

template <class T>
constexpr auto get_class_name() noexcept
{
#if defined(__clang__)
#if __clang_major__ < 12
    // Older versions of clang include inline namespaces in `__PRETTY_FUNCTION__`
    return StringView{__PRETTY_FUNCTION__ + 45, sizeof(__PRETTY_FUNCTION__) - 47};
#else
    return StringView{__PRETTY_FUNCTION__ + 42, sizeof(__PRETTY_FUNCTION__) - 44};
#endif
#elif defined(__GNUC__)
    return StringView{__PRETTY_FUNCTION__ + 60, sizeof(__PRETTY_FUNCTION__) - 62};
#elif defined(_MSC_VER)
    return StringView{__FUNCSIG__ + 52, sizeof(__FUNCSIG__) - 69};
#else
    return StringView{};
#endif
}

template <auto T>
inline constexpr auto MEMBER_FUNCTION_CLASS_NAME_V = true;

template <class R, class S, class... T, R (S::*f)(T...)>
inline constexpr auto MEMBER_FUNCTION_CLASS_NAME_V<f> = detail::get_class_name<S>();

template <auto T>
constexpr auto get_function_name() noexcept
{
#if defined(__clang__)
// Older versions of clang include inline namespaces in `__PRETTY_FUNCTION__`
#if __clang_major__ < 12
    return StringView{__PRETTY_FUNCTION__ + 49, sizeof(__PRETTY_FUNCTION__) - 51};
#else
    return StringView{__PRETTY_FUNCTION__ + 46, sizeof(__PRETTY_FUNCTION__) - 48};
#endif
#elif defined(__GNUC__)
    return StringView{__PRETTY_FUNCTION__ + 69, sizeof(__PRETTY_FUNCTION__) - 71};
#elif defined(_MSC_VER)
    // MSVC returns the entire function signature
    return StringView{__FUNCSIG__ + 49, sizeof(__FUNCSIG__) - 66};
#else
    return StringView{};
#endif
}

template <size_t N>
struct PreparedServiceName
{
    StaticString<N> string;
    size_t size;
};

template <auto PrepareAsync>
constexpr auto prepare_service_name()
{
    constexpr auto member_func_class_name = detail::MEMBER_FUNCTION_CLASS_NAME_V<PrepareAsync>;
    constexpr auto stub_suffix_size = sizeof("::Stub") - 1;
    constexpr auto service_name = member_func_class_name.substr(0, member_func_class_name.size() - stub_suffix_size);
    PreparedServiceName<service_name.size()> result{};
    const auto begin = result.string.begin();
    const auto end = result.string.end();
    detail::copy(service_name.begin(), service_name.end(), begin);
    const auto new_end = detail::replace_sequence_with_value(begin, end, StaticString{':', ':'}, '.');
    result.size = new_end - begin;
    return result;
}

template <auto PrepareAsync>
constexpr auto get_client_service_name()
{
    constexpr auto prepared = detail::prepare_service_name<PrepareAsync>();
    StaticString<prepared.size> chars{};
    detail::copy(prepared.string.begin(), prepared.string.begin() + prepared.size, chars.begin());
    return chars;
}

template <auto PrepareAsync>
inline constexpr auto CLIENT_SERVICE_NAME_V = detail::get_client_service_name<PrepareAsync>();

template <auto PrepareAsync>
constexpr auto get_client_method_name()
{
    constexpr auto function_name = detail::get_function_name<PrepareAsync>();
    constexpr auto member_func_class_name = detail::MEMBER_FUNCTION_CLASS_NAME_V<PrepareAsync>;
    constexpr auto prepare_async_size = sizeof("::PrepareAsync") - 1;
    constexpr auto begin = detail::search(function_name.begin(), function_name.end(), member_func_class_name.begin(),
                                          member_func_class_name.end()) +
                           member_func_class_name.size() + prepare_async_size;
    constexpr auto end = detail::find(begin, function_name.end(), '(');
    constexpr auto size = end - begin;
    constexpr auto method_name = function_name.substr(std::distance(function_name.begin(), begin), size);
    StaticString<method_name.size()> result{};
    detail::copy(method_name.begin(), method_name.end(), result.begin());
    return result;
}

template <auto PrepareAsync>
inline constexpr auto CLIENT_METHOD_NAME_V = detail::get_client_method_name<PrepareAsync>();
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NAME_HPP
