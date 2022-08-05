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

#ifndef AGRPC_AGRPC_BIND_ALLOCATOR_HPP
#define AGRPC_AGRPC_BIND_ALLOCATOR_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/memory_resource.hpp>
#include <agrpc/detail/utility.hpp>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Helper class that associates an allocator to an object
 *
 * This class can be used to customize the allocator of an asynchronous operation. Especially useful when the completion
 * token has an associated executor already or when `asio::require`ing a different allocator from the executor is not
 * possible:
 *
 * @snippet client.cpp bind_allocator-client-side
 *
 * In contrast to `asio::bind_allocator` this class performs empty class optimization on the provided allocator.
 *
 * @since 1.5.0
 */
template <class Target, class Allocator>
class AllocatorBinder
{
  public:
    /**
     * @brief The target type
     */
    using target_type = Target;

    /**
     * @brief The target's associated executor type
     */
    using executor_type = detail::AssociatedExecutorT<Target>;

    /**
     * @brief The bound allocator type
     */
    using allocator_type = Allocator;

    /**
     * @brief Construct from an allocator and argument pack
     *
     * Supports deduction guide when constructing from a single argument. The following creates an `AllocatorBinder<int
     * std::allocator<void>>`
     *
     * @code{.cpp}
     * agrpc::AllocatorBinder int_binder{std::allocator<void>{}, 1};
     * @endcode
     */
    template <class... Args>
    constexpr explicit AllocatorBinder(const Allocator& allocator, Args&&... args)
        : impl(detail::SecondThenVariadic{}, allocator, std::forward<Args>(args)...)
    {
    }

    /**
     * @brief Default copy constructor
     */
    AllocatorBinder(const AllocatorBinder& other) = default;

    /**
     * @brief Copy construct from a different agrpc::AllocatorBinder
     */
    template <class OtherTarget, class OtherAllocator>
    constexpr explicit AllocatorBinder(const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl(other.get(), other.impl.second())
    {
    }

    /**
     * @brief Copy construct from a different agrpc::AllocatorBinder and specified allocator
     */
    template <class OtherTarget, class OtherAllocator>
    constexpr AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl(other.get(), allocator)
    {
    }

    /**
     * @brief Copy construct from a different const agrpc::AllocatorBinder and specified allocator
     */
    template <class OtherTarget, class OtherAllocator>
    constexpr AllocatorBinder(const Allocator& allocator, const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl(other.get(), allocator)
    {
    }

    /**
     * @brief Default move constructor
     */
    AllocatorBinder(AllocatorBinder&& other) = default;

    /**
     * @brief Move construct from a different agrpc::AllocatorBinder
     */
    template <class OtherTarget, class OtherAllocator>
    constexpr explicit AllocatorBinder(AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : impl(std::move(other.get()), std::move(other.impl.second()))
    {
    }

    /**
     * @brief Move construct from a different agrpc::AllocatorBinder and specified allocator
     */
    template <class OtherTarget, class OtherAllocator>
    constexpr AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : impl(std::move(other.get()), allocator)
    {
    }

    /**
     * @brief Default destructor
     */
    ~AllocatorBinder() = default;

    /**
     * @brief Default copy assignment operator
     */
    AllocatorBinder& operator=(const AllocatorBinder& other) = default;

    /**
     * @brief Default move assignment operator
     */
    AllocatorBinder& operator=(AllocatorBinder&& other) = default;

    /**
     * @brief Get the target (mutable)
     */
    constexpr target_type& get() noexcept { return impl.first(); }

    /**
     * @brief Get the target (const)
     */
    constexpr const target_type& get() const noexcept { return impl.first(); }

    /**
     * @brief Get the target's associated executor
     */
    constexpr executor_type get_executor() const noexcept { return detail::exec::get_executor(this->get()); }

    /**
     * @brief Get the bound allocator
     */
    constexpr Allocator get_allocator() const noexcept { return impl.second(); }

#ifdef AGRPC_UNIFEX
    friend Allocator tag_invoke(unifex::tag_t<unifex::get_allocator>, const AllocatorBinder& binder) noexcept
    {
        return binder.get_allocator();
    }
#endif

    /**
     * @brief Invoke target with arguments (rvalue overload)
     */
    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) &&
    {
        return std::move(this->get())(std::forward<Args>(args)...);
    }

    /**
     * @brief Invoke target with arguments (lvalue overload)
     */
    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) &
    {
        return this->get()(std::forward<Args>(args)...);
    }

    /**
     * @brief Invoke target with arguments (const lvalue overload)
     */
    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) const&
    {
        return this->get()(std::forward<Args>(args)...);
    }

  private:
    template <class, class>
    friend class agrpc::AllocatorBinder;

    detail::CompressedPair<Target, Allocator> impl;
};

template <class Allocator, class Target>
AllocatorBinder(const Allocator& allocator, Target&& target) -> AllocatorBinder<detail::RemoveCrefT<Target>, Allocator>;

/**
 * @brief Helper function to create an AllocatorBinder
 *
 * @since 1.5.0
 */
template <class Allocator, class Target>
constexpr agrpc::AllocatorBinder<detail::RemoveCrefT<Target>, Allocator> bind_allocator(const Allocator& allocator,
                                                                                        Target&& target)
{
    return agrpc::AllocatorBinder{allocator, std::forward<Target>(target)};
}

// Implementation details
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

namespace detail
{
template <class TargetAsyncResult, class Allocator, class = void>
struct AllocatorBinderAsyncResultCompletionHandlerType
{
};

template <class TargetAsyncResult, class Allocator>
struct AllocatorBinderAsyncResultCompletionHandlerType<TargetAsyncResult, Allocator,
                                                       std::void_t<typename TargetAsyncResult::completion_handler_type>>
{
    using completion_handler_type =
        agrpc::AllocatorBinder<typename TargetAsyncResult::completion_handler_type, Allocator>;
};

template <class TargetAsyncResult, class Allocator, class = void>
struct AllocatorBinderAsyncResultHandlerType
{
};

template <class TargetAsyncResult, class Allocator>
struct AllocatorBinderAsyncResultHandlerType<TargetAsyncResult, Allocator,
                                             std::void_t<typename TargetAsyncResult::handler_type>>
{
    using handler_type = agrpc::AllocatorBinder<typename TargetAsyncResult::handler_type, Allocator>;
};

template <class TargetAsyncResult, class = void>
struct AllocatorBinderAsyncResultReturnType
{
};

template <class TargetAsyncResult>
struct AllocatorBinderAsyncResultReturnType<TargetAsyncResult, std::void_t<typename TargetAsyncResult::return_type>>
{
    using return_type = typename TargetAsyncResult::return_type;
};

template <class Initiation, class Allocator>
struct AllocatorBinderAsyncResultInitWrapper
{
    template <class Handler, class... Args>
    constexpr void operator()(Handler&& handler, Args&&... args) &&
    {
        std::move(initiation)(agrpc::AllocatorBinder(allocator, std::forward<Handler>(handler)),
                              std::forward<Args>(args)...);
    }

    Allocator allocator;
    Initiation initiation;
};
}  // namespace detail

#endif

AGRPC_NAMESPACE_END

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

template <class CompletionToken, class Allocator, class Signature>
class agrpc::asio::async_result<agrpc::AllocatorBinder<CompletionToken, Allocator>, Signature>
    : public agrpc::detail::AllocatorBinderAsyncResultCompletionHandlerType<async_result<CompletionToken, Signature>,
                                                                            Allocator>,
      public agrpc::detail::AllocatorBinderAsyncResultHandlerType<async_result<CompletionToken, Signature>, Allocator>,
      public agrpc::detail::AllocatorBinderAsyncResultReturnType<async_result<CompletionToken, Signature>>
{
  public:
    constexpr explicit async_result(agrpc::AllocatorBinder<CompletionToken, Allocator>& binder) : result(binder.get())
    {
    }

    constexpr decltype(auto) get() { return result.get(); }

    template <class Initiation, class BoundCompletionToken, class... Args>
    static decltype(auto) initiate(Initiation&& initiation, BoundCompletionToken&& token, Args&&... args)
    {
        return asio::async_initiate<CompletionToken, Signature>(
            agrpc::detail::AllocatorBinderAsyncResultInitWrapper<agrpc::detail::RemoveCrefT<Initiation>, Allocator>{
                token.get_allocator(), std::forward<Initiation>(initiation)},
            std::forward<BoundCompletionToken>(token).get(), std::forward<Args>(args)...);
    }

  private:
    async_result<CompletionToken, Signature> result;
};

template <class Target, class Allocator, class Allocator1>
struct agrpc::asio::associated_allocator<agrpc::AllocatorBinder<Target, Allocator>, Allocator1>
{
    using type = Allocator;

    static constexpr type get(const agrpc::AllocatorBinder<Target, Allocator>& b,
                              const Allocator1& = Allocator1()) noexcept
    {
        return b.get_allocator();
    }
};

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <template <class, class> class Associator, class Target, class Allocator, class DefaultCandidate>
struct agrpc::asio::associator<Associator, agrpc::AllocatorBinder<Target, Allocator>, DefaultCandidate>
{
    using type = typename Associator<Target, DefaultCandidate>::type;

    static constexpr type get(const agrpc::AllocatorBinder<Target, Allocator>& b,
                              const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<Target, DefaultCandidate>::get(b.get(), c);
    }
};

#endif

#endif

template <class Allocator, class Target, class Alloc>
struct agrpc::detail::container::uses_allocator<agrpc::AllocatorBinder<Allocator, Target>, Alloc> : std::false_type
{
};

#endif  // AGRPC_AGRPC_BIND_ALLOCATOR_HPP
