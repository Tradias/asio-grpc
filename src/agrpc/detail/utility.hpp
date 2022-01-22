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

#ifndef AGRPC_DETAIL_UTILITY_HPP
#define AGRPC_DETAIL_UTILITY_HPP

#include "agrpc/detail/config.hpp"

#include <type_traits>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
using RemoveCvrefT = std::remove_cv_t<std::remove_reference_t<T>>;

struct Empty
{
    Empty() = default;
    Empty(const Empty&) = default;
    Empty(Empty&&) = default;
    Empty& operator=(const Empty&) = default;
    Empty& operator=(Empty&&) = default;
    ~Empty() = default;

    template <class Arg, class... Args>
    constexpr explicit Empty(Arg&&, Args&&...) noexcept
    {
    }
};

template <class First, class Second, bool = std::is_empty_v<Second> && !std::is_final_v<Second>>
class CompressedPair : private Second
{
  public:
    CompressedPair() = default;

    template <class T, class U>
    constexpr CompressedPair(T&& first, U&& second) noexcept(
        std::is_nothrow_constructible_v<First, T&&>&& std::is_nothrow_constructible_v<Second, U&&>)
        : Second(std::forward<U>(second)), first_(std::forward<T>(first))
    {
    }

    template <class T>
    constexpr explicit CompressedPair(T&& first) noexcept(
        std::is_nothrow_constructible_v<First, T&&>&& std::is_nothrow_default_constructible_v<Second>)
        : Second(), first_(std::forward<T>(first))
    {
    }

    constexpr auto& first() noexcept { return this->first_; }

    constexpr const auto& first() const noexcept { return this->first_; }

    constexpr Second& second() noexcept { return *this; }

    constexpr const Second& second() const noexcept { return *this; }

  private:
    First first_;
};

template <class First, class Second>
class CompressedPair<First, Second, false>
{
  public:
    CompressedPair() = default;

    template <class T, class U>
    constexpr CompressedPair(T&& first, U&& second) noexcept(
        std::is_nothrow_constructible_v<First, T&&>&& std::is_nothrow_constructible_v<Second, U&&>)
        : first_(std::forward<T>(first)), second_(std::forward<U>(second))
    {
    }

    template <class T>
    constexpr explicit CompressedPair(T&& first) noexcept(
        std::is_nothrow_constructible_v<First, T&&>&& std::is_nothrow_default_constructible_v<Second>)
        : first_(std::forward<T>(first)), second_()
    {
    }

    constexpr auto& first() noexcept { return this->first_; }

    constexpr const auto& first() const noexcept { return this->first_; }

    constexpr auto& second() noexcept { return this->second_; }

    constexpr const auto& second() const noexcept { return this->second_; }

  private:
    First first_;
    Second second_;
};

template <class T, bool = std::is_empty_v<T> && !std::is_final_v<T>>
class EmptyBaseOptimization : private T
{
  public:
    template <class... Args>
    explicit EmptyBaseOptimization(Args&&... args) : T(std::forward<Args>(args)...)
    {
    }

    constexpr auto& get() noexcept { return static_cast<T&>(*this); }

    constexpr auto& get() const noexcept { return static_cast<const T&>(*this); }
};

template <class T>
class EmptyBaseOptimization<T, false>
{
  public:
    template <class... Args>
    explicit EmptyBaseOptimization(Args&&... args) : value(std::forward<Args>(args)...)
    {
    }

    constexpr auto& get() noexcept { return this->value; }

    constexpr auto& get() const noexcept { return this->value; }

  private:
    T value;
};

template <class OnExit>
struct ScopeGuard
{
    OnExit on_exit;
    bool is_armed{true};

    constexpr explicit ScopeGuard(OnExit on_exit) : on_exit(std::move(on_exit)) {}

    ScopeGuard(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&&) = delete;

    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard& operator=(ScopeGuard&&) = delete;

    ~ScopeGuard() noexcept
    {
        if (is_armed)
        {
            on_exit();
        }
    }

    constexpr void release() noexcept { is_armed = false; }
};

struct InplaceWithFunction
{
};

template <class T>
struct InplaceWithFunctionWrapper
{
    T value;

    template <class... Args>
    explicit InplaceWithFunctionWrapper(Args&&... args) : value(std::forward<Args>(args)...)
    {
    }

    template <class Function>
    InplaceWithFunctionWrapper(detail::InplaceWithFunction, Function&& function)
        : value(std::forward<Function>(function)())
    {
    }
};

template <class T>
T forward_as(std::add_lvalue_reference_t<std::remove_reference_t<T>> u)
{
    if constexpr (std::is_rvalue_reference_v<T> || !std::is_reference_v<T>)
    {
        return std::move(u);
    }
    else
    {
        return u;
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_UTILITY_HPP
