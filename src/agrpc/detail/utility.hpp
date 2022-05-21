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

#include <agrpc/detail/config.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
using RemoveCvrefT = std::remove_cv_t<std::remove_reference_t<T>>;

template <class T, class = void>
inline constexpr bool IS_EQUALITY_COMPARABLE = false;

template <class T>
inline constexpr bool IS_EQUALITY_COMPARABLE<
    T, std::void_t<decltype(static_cast<bool>(std::declval<const T&>() == std::declval<const T&>())),
                   decltype(static_cast<bool>(std::declval<const T&>() != std::declval<const T&>()))>> = true;

template <class Function, class Signature>
struct InvokeResultFromSignature;

template <class Function, class... Args>
struct InvokeResultFromSignature<Function, void(Args...)>
{
    using Type = std::invoke_result_t<Function, Args...>;
};

template <class Function, class Signature>
using InvokeResultFromSignatureT = typename detail::InvokeResultFromSignature<Function, Signature>::Type;

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
    Empty() = default;

    template <class Arg, class... Args>
    constexpr explicit Empty(Arg&&, Args&&...) noexcept
    {
    }
};

struct NoOp
{
    template <class... Args>
    constexpr void operator()(Args&&...) const noexcept
    {
    }
};

struct InplaceWithFunction
{
};

struct SecondThenVariadic
{
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

    template <class U, class... Args>
    constexpr CompressedPair(detail::SecondThenVariadic, U&& second, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<First, Args&&...>&& std::is_nothrow_constructible_v<Second, U&&>)
        : Second(std::forward<U>(second)), first_(std::forward<Args>(args)...)
    {
    }

    constexpr First& first() noexcept { return this->first_; }

    constexpr const First& first() const noexcept { return this->first_; }

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

    template <class U, class... Args>
    constexpr CompressedPair(detail::SecondThenVariadic, U&& second, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<First, Args&&...>&& std::is_nothrow_constructible_v<Second, U&&>)
        : first_(std::forward<Args>(args)...), second_(std::forward<U>(second))
    {
    }

    constexpr First& first() noexcept { return this->first_; }

    constexpr const First& first() const noexcept { return this->first_; }

    constexpr Second& second() noexcept { return this->second_; }

    constexpr const Second& second() const noexcept { return this->second_; }

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

    template <class Function>
    EmptyBaseOptimization(detail::InplaceWithFunction, Function&& function) : T(std::forward<Function>(function)())
    {
    }

    constexpr T& get() noexcept { return static_cast<T&>(*this); }

    constexpr const T& get() const noexcept { return static_cast<const T&>(*this); }
};

template <class T>
class EmptyBaseOptimization<T, false>
{
  public:
    template <class... Args>
    explicit EmptyBaseOptimization(Args&&... args) : value(std::forward<Args>(args)...)
    {
    }

    template <class Function>
    EmptyBaseOptimization(detail::InplaceWithFunction, Function&& function) : value(std::forward<Function>(function)())
    {
    }

    constexpr T& get() noexcept { return this->value; }

    constexpr const T& get() const noexcept { return this->value; }

  private:
    T value;
};

template <class OnExit>
class ScopeGuard
{
  public:
    constexpr explicit ScopeGuard(OnExit on_exit) : on_exit(std::move(on_exit)) {}

    template <class... Args>
    constexpr explicit ScopeGuard(Args&&... args) : on_exit(std::forward<Args>(args)...)
    {
    }

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

  private:
    OnExit on_exit;
    bool is_armed{true};
};

template <class T>
struct InplaceWithFunctionWrapper
{
    template <class... Args>
    explicit InplaceWithFunctionWrapper(Args&&... args) : value(std::forward<Args>(args)...)
    {
    }

    template <class Function>
    InplaceWithFunctionWrapper(detail::InplaceWithFunction, Function&& function)
        : value(std::forward<Function>(function)())
    {
    }

    T value;
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

template <class Function, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_impl(Function&& function, Tuple&& tuple, std::index_sequence<I...>)
{
    return std::forward<Function>(function)(std::get<I>(std::forward<Tuple>(tuple))...);
}

template <class Function, template <class...> class Tuple, class... T>
constexpr decltype(auto) apply(Function&& function, Tuple<T...>&& tuple)
{
    return detail::apply_impl(std::forward<Function>(function), std::move(tuple),
                              std::make_index_sequence<sizeof...(T)>{});
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_UTILITY_HPP
