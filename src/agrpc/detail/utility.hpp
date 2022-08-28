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
template <class...>
struct TypeList
{
};

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

template <class T>
struct TypeIdentity
{
    using Type = T;
};

template <class T>
using TypeIdentityT = typename detail::TypeIdentity<T>::Type;

#ifdef AGRPC_HAS_CONCEPTS
template <class T>
concept IS_EQUALITY_COMPARABLE = requires(const T& lhs, const T& rhs)
{
    {static_cast<bool>(lhs == rhs)};
    {static_cast<bool>(lhs != rhs)};
};
#else
template <class T, class = void>
inline constexpr bool IS_EQUALITY_COMPARABLE = false;

template <class T>
inline constexpr bool IS_EQUALITY_COMPARABLE<
    T, std::void_t<decltype(static_cast<bool>(std::declval<const T&>() == std::declval<const T&>())),
                   decltype(static_cast<bool>(std::declval<const T&>() != std::declval<const T&>()))>> = true;
#endif

template <class Signature>
struct InvokeResultFromSignature;

template <class... Args>
struct InvokeResultFromSignature<void(Args...)>
{
    template <class Function>
    using Type = std::invoke_result_t<Function, Args...>;
};

template <class Function, class Signature>
using InvokeResultFromSignatureT = typename detail::InvokeResultFromSignature<Signature>::template Type<Function>;

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

struct NoOp
{
    template <class... Args>
    constexpr void operator()(Args&&...) const noexcept
    {
    }
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

    constexpr First& first() noexcept { return this->first_; }

    constexpr const First& first() const noexcept { return this->first_; }

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
    constexpr explicit CompressedPair(T&& first) : first_(static_cast<T&&>(first)), second_{}
    {
    }

    template <class U, class... Args>
    constexpr CompressedPair(detail::SecondThenVariadic, U&& second, Args&&... args)
        : first_(static_cast<Args&&>(args)...), second_(static_cast<U&&>(second))
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

template <class First, class Second>
CompressedPair(First, Second) -> CompressedPair<First, Second>;

struct InplaceWithFunction
{
};

template <class T, bool = std::is_empty_v<T> && !std::is_final_v<T>>
class EmptyBaseOptimization : private T
{
  public:
    template <class... Args>
    constexpr explicit EmptyBaseOptimization(Args&&... args) : T(static_cast<Args&&>(args)...)
    {
    }

    template <class Function>
    constexpr EmptyBaseOptimization(detail::InplaceWithFunction, Function&& function)
        : T(static_cast<Function&&>(function)())
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
    constexpr explicit EmptyBaseOptimization(Args&&... args) : value(static_cast<Args&&>(args)...)
    {
    }

    template <class Function>
    constexpr EmptyBaseOptimization(detail::InplaceWithFunction, Function&& function)
        : value(static_cast<Function&&>(function)())
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
    constexpr explicit ScopeGuard(OnExit on_exit) : on_exit(static_cast<OnExit&&>(on_exit)) {}

    template <class... Args>
    constexpr explicit ScopeGuard(Args&&... args) : on_exit(static_cast<Args&&>(args)...)
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

    constexpr void fire() noexcept
    {
        this->release();
        on_exit();
    }

  private:
    OnExit on_exit;
    bool is_armed{true};
};

template <class T>
struct InplaceWithFunctionWrapper
{
    template <class... Args>
    constexpr explicit InplaceWithFunctionWrapper(Args&&... args) : value(static_cast<Args&&>(args)...)
    {
    }

    template <class Function>
    constexpr InplaceWithFunctionWrapper(detail::InplaceWithFunction, Function&& function)
        : value(static_cast<Function&&>(function)())
    {
    }

    T value;
};

template <class T>
inline constexpr bool IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V = std::is_nothrow_constructible_v<detail::RemoveCrefT<T>, T>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_UTILITY_HPP
