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

#ifndef AGRPC_DETAIL_UTILITY_HPP
#define AGRPC_DETAIL_UTILITY_HPP

#include <type_traits>
#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class...>
struct TypeList
{
};

#if defined(__cpp_lib_remove_cvref) && (__cpp_lib_remove_cvref >= 201711L)
template <class T>
using RemoveCrefT = std::remove_cvref_t<T>;
#else
template <class T>
struct RemoveCref
{
    using Type = T;
};

template <class T>
struct RemoveCref<const T>
{
    using Type = T;
};

template <class T>
struct RemoveCref<T&>
{
    using Type = T;
};

template <class T>
struct RemoveCref<const T&>
{
    using Type = T;
};

template <class T>
struct RemoveCref<T&&>
{
    using Type = T;
};

template <class T>
struct RemoveCref<const T&&>
{
    using Type = T;
};

template <class T>
using RemoveCrefT = typename detail::RemoveCref<T>::Type;
#endif

template <class T, class = void>
inline constexpr bool IS_EQUALITY_COMPARABLE = false;

template <class T>
inline constexpr bool IS_EQUALITY_COMPARABLE<
    T, std::void_t<decltype(static_cast<bool>(std::declval<const T&>() == std::declval<const T&>())),
                   decltype(static_cast<bool>(std::declval<const T&>() != std::declval<const T&>()))>> = true;

template <class...>
inline constexpr bool ALWAYS_FALSE = false;

template <bool>
struct Conditional
{
    template <class T, class>
    using Type = T;
};

template <>
struct Conditional<false>
{
    template <class, class U>
    using Type = U;
};

template <bool Condition, class T, class U>
using ConditionalT = typename Conditional<Condition>::template Type<T, U>;

struct Empty
{
};

struct SecondThenVariadic
{
};

template <class First, class Second, bool = std::is_empty_v<Second> && !std::is_final_v<Second>>
class CompressedPair final : private Second
{
  public:
    CompressedPair() = default;

    template <class T, class U>
    constexpr CompressedPair(T&& first, U&& second) : Second(static_cast<U&&>(second)), first_(static_cast<T&&>(first))
    {
    }

    template <class T, class = std::enable_if_t<(!std::is_same_v<CompressedPair, detail::RemoveCrefT<T>>)>>
    constexpr explicit CompressedPair(T&& first) : Second{}, first_(static_cast<T&&>(first))
    {
    }

    template <class U, class... Args>
    constexpr CompressedPair(detail::SecondThenVariadic, U&& second, Args&&... args)
        : Second(static_cast<U&&>(second)), first_(static_cast<Args&&>(args)...)
    {
    }

    constexpr First& first() noexcept { return first_; }

    constexpr const First& first() const noexcept { return first_; }

    constexpr Second& second() noexcept { return *this; }

    constexpr const Second& second() const noexcept { return *this; }

  private:
    First first_;
};

template <class First, class Second>
class CompressedPair<First, Second, false> final
{
  public:
    CompressedPair() = default;

    template <class T, class U>
    constexpr CompressedPair(T&& first, U&& second) : first_(static_cast<T&&>(first)), second_(static_cast<U&&>(second))
    {
    }

    template <class T, class = std::enable_if_t<(!std::is_same_v<CompressedPair, detail::RemoveCrefT<T>>)>>
    constexpr explicit CompressedPair(T&& first) : first_(static_cast<T&&>(first))
    {
    }

    template <class U, class... Args>
    constexpr CompressedPair(detail::SecondThenVariadic, U&& second, Args&&... args)
        : first_(static_cast<Args&&>(args)...), second_(static_cast<U&&>(second))
    {
    }

    constexpr First& first() noexcept { return first_; }

    constexpr const First& first() const noexcept { return first_; }

    constexpr Second& second() noexcept { return second_; }

    constexpr const Second& second() const noexcept { return second_; }

  private:
    First first_;
    Second second_{};
};

template <class First, class Second>
CompressedPair(First, Second) -> CompressedPair<First, Second>;

template <class OnExit>
class ScopeGuard
{
  public:
    constexpr explicit ScopeGuard(OnExit on_exit) : on_exit_{static_cast<OnExit&&>(on_exit)} {}

    template <class... Args>
    constexpr explicit ScopeGuard(Args&&... args) : on_exit_{static_cast<Args&&>(args)...}
    {
    }

    ScopeGuard(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept
        : on_exit_(static_cast<OnExit&&>(other.on_exit_)), is_armed_(std::exchange(other.is_armed_, false))
    {
    }

    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

    ~ScopeGuard() noexcept
    {
        if (is_armed_)
        {
            on_exit_();
        }
    }

    constexpr void release() noexcept { is_armed_ = false; }

    constexpr OnExit& get() noexcept { return on_exit_; }

    constexpr const OnExit& get() const noexcept { return on_exit_; }

    constexpr void execute()
    {
        if (is_armed_)
        {
            is_armed_ = false;
            on_exit_();
        }
    }

  private:
    OnExit on_exit_;
    bool is_armed_{true};
};

struct InplaceWithFunction
{
};

template <class T>
struct InplaceWithFunctionWrapper
{
    template <class... Args>
    constexpr explicit InplaceWithFunctionWrapper(Args&&... args) : value_(static_cast<Args&&>(args)...)
    {
    }

    template <class Function>
    constexpr InplaceWithFunctionWrapper(detail::InplaceWithFunction, Function&& function)
        : value_(static_cast<Function&&>(function)())
    {
    }

    T value_;
};

template <class T>
inline constexpr bool IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V = std::is_nothrow_constructible_v<detail::RemoveCrefT<T>, T>;

template <class Enum>
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept
{
    return static_cast<std::underlying_type_t<Enum>>(e);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_UTILITY_HPP
