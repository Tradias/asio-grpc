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

#ifndef AGRPC_AGRPC_BINDALLOCATOR_HPP
#define AGRPC_AGRPC_BINDALLOCATOR_HPP

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/utility.hpp"

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
    using executor_type = asio::associated_executor_t<Target>;

    /**
     * @brief The bound allocator type
     */
    using allocator_type = Allocator;

    /**
     * @brief Construct from an allocator and argument pack
     *
     * Supports deduction guide when constructing from a single argument. The following creates an `AllocatorBinder<int
     * std::allocator<std::byte>>`
     *
     * @code{.cpp}
     * agrpc::AllocatorBinder int_binder{std::allocator<std::byte>{}, 1};
     * @endcode
     */
    template <class... Args>
    explicit AllocatorBinder(const Allocator& allocator, Args&&... args)
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
    explicit AllocatorBinder(const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl(other.get(), other.get_allocator())
    {
    }

    /**
     * @brief Copy construct from a different agrpc::AllocatorBinder and specified allocator
     */
    template <class OtherTarget, class OtherAllocator>
    AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl(other.get(), allocator)
    {
    }

    /**
     * @brief Copy construct from a different const agrpc::AllocatorBinder and specified allocator
     */
    template <class OtherTarget, class OtherAllocator>
    AllocatorBinder(const Allocator& allocator, const AllocatorBinder<OtherTarget, OtherAllocator>& other)
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
    explicit AllocatorBinder(AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : impl(std::move(other.get()), std::move(other.get_allocator()))
    {
    }

    /**
     * @brief Move construct from a different agrpc::AllocatorBinder and specified allocator
     */
    template <class OtherTarget, class OtherAllocator>
    AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>&& other)
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
    target_type& get() noexcept { return impl.first(); }

    /**
     * @brief Get the target (const)
     */
    const target_type& get() const noexcept { return impl.first(); }

    /**
     * @brief Get the target's associated executor
     */
    executor_type get_executor() const noexcept { return asio::get_associated_executor(this->get()); }

    /**
     * @brief Get the bound allocator
     */
    allocator_type get_allocator() const noexcept { return impl.second(); }

    /**
     * @brief Invoke target with arguments (rvalue overload)
     */
    template <class... Args>
    decltype(auto) operator()(Args&&... args) &&
    {
        return std::move(this->get())(std::forward<Args>(args)...);
    }

    /**
     * @brief Invoke target with arguments (lvalue overload)
     */
    template <class... Args>
    decltype(auto) operator()(Args&&... args) &
    {
        return this->get()(std::forward<Args>(args)...);
    }

    /**
     * @brief Invoke target with arguments (const lvalue overload)
     */
    template <class... Args>
    decltype(auto) operator()(Args&&... args) const&
    {
        return this->get()(std::forward<Args>(args)...);
    }

  private:
    detail::CompressedPair<Target, Allocator> impl;
};

template <class Allocator, class Target>
AllocatorBinder(const Allocator& allocator, Target&& target)
    -> AllocatorBinder<detail::RemoveCvrefT<Target>, Allocator>;

/**
 * @brief Helper function to create an agrpc::AllocatorBinder
 *
 * @since 1.5.0
 */
template <class Allocator, class Target>
auto bind_allocator(const Allocator& allocator, Target&& target)
{
    return agrpc::AllocatorBinder{allocator, std::forward<Target>(target)};
}

AGRPC_NAMESPACE_END

#include "agrpc/detail/bindAllocator.ipp"

#endif

#endif  // AGRPC_AGRPC_BINDALLOCATOR_HPP
