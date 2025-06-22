// Copyright 2025 Dennis Hezel
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
#include <agrpc/detail/utility.hpp>

#include <string_view>
#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <std::size_t N>
struct FixedSizeString
{
    // Always null-terminated
    char data_[N + 1];

    [[nodiscard]] constexpr std::string_view view() const noexcept { return {data_, N}; }

    [[nodiscard]] static constexpr auto size() noexcept { return N; }

    [[nodiscard]] constexpr auto begin() noexcept { return data_; }

    [[nodiscard]] constexpr auto begin() const noexcept { return data_; }

    [[nodiscard]] constexpr auto end() noexcept { return data_ + N; }

    [[nodiscard]] constexpr auto end() const noexcept { return data_ + N; }
};

template <class... U>
FixedSizeString(U...) -> FixedSizeString<sizeof...(U)>;

template <std::size_t N>
struct StaticString
{
    char data_[N];
    std::size_t size_{N};

    [[nodiscard]] constexpr auto begin() noexcept { return data_; }

    [[nodiscard]] constexpr auto begin() const noexcept { return data_; }

    [[nodiscard]] constexpr auto end() noexcept { return data_ + size_; }

    [[nodiscard]] constexpr auto end() const noexcept { return data_ + size_; }

    [[nodiscard]] constexpr auto size() const noexcept { return size_; }

    constexpr void set_size(std::size_t new_size) noexcept { size_ = new_size; }
};

struct StringView
{
    const char* data_;
    std::size_t size_;

    [[nodiscard]] constexpr auto size() const noexcept { return size_; }

    [[nodiscard]] constexpr auto begin() const noexcept { return data_; }

    [[nodiscard]] constexpr auto end() const noexcept { return data_ + size_; }

    [[nodiscard]] constexpr auto substr(std::size_t pos, std::size_t count) const noexcept
    {
        return StringView{data_ + pos, count};
    }
};

template <class T>
constexpr auto get_class_name()
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
    return StringView{__FUNCSIG__ + 52, sizeof(__FUNCSIG__) - 60};
#else
    static_assert(detail::ALWAYS_FALSE<T>, "compiler not supported");
#endif
}

template <auto T>
inline constexpr auto MEMBER_FUNCTION_CLASS_NAME_V = true;

template <class R, class S, class... T, R (S::*F)(T...)>
inline constexpr auto MEMBER_FUNCTION_CLASS_NAME_V<F> = detail::get_class_name<S>();

template <auto T>
constexpr auto get_function_name()
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
    return StringView{__FUNCSIG__ + 49, sizeof(__FUNCSIG__) - 57};
#else
    static_assert(detail::ALWAYS_FALSE<T>, "compiler not supported");
#endif
}

template <class T>
constexpr auto prepare_service_name()
{
    auto result = T::prepare_service_name();
    const auto begin = result.begin();
    const auto end = result.end();
    const auto new_end = detail::replace_sequence_with_value(begin, end, FixedSizeString{':', ':'}, '.');
    result.set_size(new_end - begin);
    return result;
}

template <class T>
constexpr auto get_service_name()
{
    constexpr auto prepared = detail::prepare_service_name<T>();
    FixedSizeString<prepared.size()> result{};
    detail::copy(prepared.begin(), prepared.end(), result.begin());
    return result;
}

template <auto PrepareAsync>
struct ClientName
{
    static constexpr auto FUNCTION = PrepareAsync;
    static constexpr const char METHOD_PREFIX[] = "::PrepareAsync";

    static constexpr auto prepare_service_name()
    {
        constexpr auto member_func_class_name = detail::MEMBER_FUNCTION_CLASS_NAME_V<PrepareAsync>;
        constexpr auto suffix_size = sizeof("::Stub") - 1;
        constexpr auto service_name = member_func_class_name.substr(0, member_func_class_name.size() - suffix_size);
        StaticString<service_name.size()> result{};
        detail::copy(service_name.begin(), service_name.end(), result.begin());
        return result;
    }
};

template <auto PrepareAsync>
inline constexpr auto CLIENT_SERVICE_NAME_V = detail::get_service_name<ClientName<PrepareAsync>>();

template <auto RequestRPC>
struct ServerName
{
    static constexpr auto FUNCTION = RequestRPC;
    static constexpr const char METHOD_PREFIX[] = "::Request";

    static constexpr auto prepare_service_name()
    {
        constexpr auto member_func_class_name = detail::MEMBER_FUNCTION_CLASS_NAME_V<RequestRPC>;
        constexpr auto first_angle_bracket =
            detail::find(member_func_class_name.begin(), member_func_class_name.end(), '<');
        constexpr auto end_of_service_name =
            detail::rfind(member_func_class_name.begin(), first_angle_bracket, ':') - 1;
        constexpr auto service_name =
            member_func_class_name.substr(0, end_of_service_name - member_func_class_name.begin());
        StaticString<service_name.size()> result{};
        detail::copy(service_name.begin(), service_name.end(), result.begin());
        return result;
    }
};

template <auto RequestRPC>
inline constexpr auto SERVER_SERVICE_NAME_V = detail::get_service_name<ServerName<RequestRPC>>();

template <class T>
constexpr auto get_method_name()
{
    constexpr auto function_name = detail::get_function_name<T::FUNCTION>();
    constexpr auto member_func_class_name = detail::MEMBER_FUNCTION_CLASS_NAME_V<T::FUNCTION>;
    constexpr auto method_prefix_size = sizeof(T::METHOD_PREFIX) - 1;
    constexpr auto begin = detail::search(function_name.begin(), function_name.end(), member_func_class_name.begin(),
                                          member_func_class_name.end()) +
                           member_func_class_name.size() + method_prefix_size;
    constexpr auto end = detail::find(begin, function_name.end(), '(');
    constexpr auto size = end - begin;
    constexpr auto method_name = function_name.substr(std::distance(function_name.begin(), begin), size);
    FixedSizeString<method_name.size()> result{};
    detail::copy(method_name.begin(), method_name.end(), result.begin());
    return result;
}

template <auto PrepareAsync>
inline constexpr auto CLIENT_METHOD_NAME_V = detail::get_method_name<ClientName<PrepareAsync>>();

template <auto RequestRPC>
inline constexpr auto SERVER_METHOD_NAME_V = detail::get_method_name<ServerName<RequestRPC>>();
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NAME_HPP
