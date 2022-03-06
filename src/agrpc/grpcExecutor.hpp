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

#ifndef AGRPC_AGRPC_GRPCEXECUTOR_HPP
#define AGRPC_AGRPC_GRPCEXECUTOR_HPP

#include "agrpc/detail/allocateOperation.hpp"
#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"
#include "agrpc/detail/grpcExecutorBase.hpp"
#include "agrpc/detail/grpcExecutorOptions.hpp"
#include "agrpc/detail/memoryResource.hpp"
#include "agrpc/detail/scheduleSender.hpp"
#include "agrpc/grpcContext.hpp"

#include <cstddef>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief GrpcContext's executor
 *
 * A lightweight handle to a GrpcContext. Trivially copyable if it is not tracking outstanding work.
 *
 * Satisfies the [Executor and Networking
 * TS](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors)
 * and [Scheduler](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/Scheduler.html) requirements and
 * can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.
 */
template <class Allocator, std::uint32_t Options>
class BasicGrpcExecutor
    : public std::conditional_t<detail::is_outstanding_work_tracked(Options),
                                detail::GrpcExecutorWorkTrackerBase<Allocator>, detail::GrpcExecutorBase<Allocator>>
{
  private:
    using Base =
        std::conditional_t<detail::is_outstanding_work_tracked(Options), detail::GrpcExecutorWorkTrackerBase<Allocator>,
                           detail::GrpcExecutorBase<Allocator>>;

  public:
    /**
     * @brief The associated allocator type
     */
    using allocator_type = Allocator;

    constexpr explicit BasicGrpcExecutor(agrpc::GrpcContext& grpc_context) noexcept(
        std::is_nothrow_default_constructible_v<allocator_type>)
        : BasicGrpcExecutor(grpc_context, allocator_type{})
    {
    }

    constexpr BasicGrpcExecutor(agrpc::GrpcContext& grpc_context, allocator_type allocator) noexcept
        : Base(&grpc_context, std::move(allocator))
    {
    }

    /**
     * @brief Get the underlying GrpcContext
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr agrpc::GrpcContext& context() const noexcept { return *this->grpc_context(); }

    /**
     * @brief Get the associated allocator
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr allocator_type get_allocator() const
        noexcept(std::is_nothrow_copy_constructible_v<allocator_type>)
    {
        return this->allocator();
    }

    /**
     * @brief Compare two GrpcExecutor for equality
     *
     * Returns true if the two executors can be interchanged with identical effects.
     *
     * Thread-safe
     */
    template <std::uint32_t OtherOptions>
    [[nodiscard]] friend constexpr bool operator==(
        const BasicGrpcExecutor& lhs, const agrpc::BasicGrpcExecutor<Allocator, OtherOptions>& rhs) noexcept
    {
        if constexpr (Options != OtherOptions)
        {
            return false;
        }
        else
        {
            return lhs.grpc_context() == rhs.grpc_context() && lhs.allocator() == rhs.allocator();
        }
    }

    /**
     * @brief Compare two GrpcExecutor for inequality
     *
     * Returns true if interchanging the two executors may not lead to identical effects.
     *
     * Thread-safe
     */
    template <std::uint32_t OtherOptions>
    [[nodiscard]] friend constexpr bool operator!=(
        const BasicGrpcExecutor& lhs, const agrpc::BasicGrpcExecutor<Allocator, OtherOptions>& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Determine whether the GrpcContext is running in the current thread
     *
     * Thread-safe
     */
    [[nodiscard]] bool running_in_this_thread() const noexcept
    {
        return detail::GrpcContextImplementation::running_in_this_thread(this->context());
    }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly, it is intended to be used by the
     * [asio::dispatch](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/dispatch.html) free
     * function.
     *
     * Thread-safe
     */
    template <class Function, class OtherAllocator>
    void dispatch(Function&& function, OtherAllocator other_allocator) const
    {
        detail::create_and_submit_no_arg_operation_if_not_stopped<false>(
            this->context(), std::forward<Function>(function), other_allocator);
    }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly, it is intended to be used by the
     * [asio::post](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/post.html) free
     * function.
     *
     * Thread-safe
     */
    template <class Function, class OtherAllocator>
    void post(Function&& function, OtherAllocator other_allocator) const
    {
        detail::create_and_submit_no_arg_operation_if_not_stopped<true>(
            this->context(), std::forward<Function>(function), other_allocator);
    }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly, it is intended to be used by the
     * [asio::defer](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/defer.html) free
     * function.
     *
     * Thread-safe
     */
    template <class Function, class OtherAllocator>
    void defer(Function&& function, OtherAllocator other_allocator) const
    {
        detail::create_and_submit_no_arg_operation_if_not_stopped<true>(
            this->context(), std::forward<Function>(function), other_allocator);
    }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::execution::execute](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__execute.html)
     * customisation point.
     *
     * Thread-safe
     */
    template <class Function>
    void execute(Function&& function) const
    {
        detail::create_and_submit_no_arg_operation_if_not_stopped<detail::is_blocking_never(Options)>(
            this->context(), std::forward<Function>(function), this->allocator());
    }

    /**
     * @brief Create a Sender that completes on the GrpcContext
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::execution::schedule](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/execution__schedule.html)
     * customisation point.
     *
     * Thread-safe
     */
    constexpr auto schedule() const noexcept { return detail::ScheduleSender{this->context()}; }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    /**
     * @brief Obtain an executor with the blocking.possibly property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::blocking_t::possibly_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_blocking_never(Options, false)>
    {
        return {this->context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the blocking.never property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::blocking_t::never_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_blocking_never(Options, true)>
    {
        return {this->context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the relationship.fork property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::prefer](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/prefer.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto prefer(asio::execution::relationship_t::fork_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_relationship_continuation(Options, false)>
    {
        return {this->context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the relationship.continuation property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::prefer](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/prefer.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto prefer(asio::execution::relationship_t::continuation_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_relationship_continuation(Options, true)>
    {
        return {this->context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the outstanding_work.tracked property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::outstanding_work_t::tracked_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_outstanding_work_tracked(Options, true)>
    {
        return {this->context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the outstanding_work.untracked property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::outstanding_work_t::untracked_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_outstanding_work_tracked(Options, false)>
    {
        return {this->context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the specified allocator property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    template <class OtherAllocator>
    [[nodiscard]] constexpr auto require(asio::execution::allocator_t<OtherAllocator> other_allocator) const noexcept
        -> agrpc::BasicGrpcExecutor<OtherAllocator, Options>
    {
        return {this->context(), other_allocator.value()};
    }

    /**
     * @brief Obtain an executor with the default allocator property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::allocator_t<void>) const noexcept
        -> agrpc::BasicGrpcExecutor<std::allocator<void>, Options>
    {
        return agrpc::BasicGrpcExecutor<std::allocator<void>, Options>{this->context()};
    }

    /**
     * @brief Query the current value of the blocking property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr asio::execution::blocking_t query(asio::execution::blocking_t) noexcept
    {
        if constexpr (detail::is_blocking_never(Options))
        {
            return asio::execution::blocking_t::never;
        }
        else
        {
            return asio::execution::blocking_t::possibly;
        }
    }

    /**
     * @brief Query the current value of the mapping property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr asio::execution::mapping_t query(asio::execution::mapping_t) noexcept
    {
        return asio::execution::mapping_t::thread;
    }

    /**
     * @brief Query the current value of the context property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr agrpc::GrpcContext& query(asio::execution::context_t) const noexcept
    {
        return this->context();
    }

    /**
     * @brief Query the current value of the relationship property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr asio::execution::relationship_t query(asio::execution::relationship_t) noexcept
    {
        if constexpr (detail::is_relationship_continuation(Options))
        {
            return asio::execution::relationship_t::continuation;
        }
        else
        {
            return asio::execution::relationship_t::fork;
        }
    }

    /**
     * @brief Query the current value of the outstanding_work property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr asio::execution::outstanding_work_t query(
        asio::execution::outstanding_work_t) noexcept
    {
        if constexpr (detail::is_outstanding_work_tracked(Options))
        {
            return asio::execution::outstanding_work_t::tracked;
        }
        else
        {
            return asio::execution::outstanding_work_t::untracked;
        }
    }

    /**
     * @brief Query the current value of the allocator property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    template <class OtherAllocator>
    [[nodiscard]] constexpr allocator_type query(asio::execution::allocator_t<OtherAllocator>) const noexcept
    {
        return this->allocator();
    }
#endif
};

/**
 * @brief Default GrpcExecutor
 *
 * The default GrpcExecutor does not track outstanding work, has the relationship.fork and blocking.never properties and
 * uses the default allocator (`std::allocator<void>`).
 */
using GrpcExecutor = agrpc::BasicGrpcExecutor<>;

namespace pmr
{
/**
 * @brief BasicGrpcExecutor specialized on `pmr::polymorphic_allocator`
 *
 * This BasicGrpcExecutor does not track outstanding work, has the relationship.fork and blocking.never properties and
 * uses the `pmr::polymorphic_allocator` allocator.
 */
using GrpcExecutor = agrpc::BasicGrpcExecutor<agrpc::detail::pmr::polymorphic_allocator<std::byte>>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_GRPCEXECUTOR_HPP
