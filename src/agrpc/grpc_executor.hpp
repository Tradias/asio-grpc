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

#ifndef AGRPC_AGRPC_GRPC_EXECUTOR_HPP
#define AGRPC_AGRPC_GRPC_EXECUTOR_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/create_and_submit_no_arg_operation.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_executor_base.hpp>
#include <agrpc/detail/grpc_executor_options.hpp>
#include <agrpc/detail/schedule_sender.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <cstddef>
#include <memory>
#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief GrpcContext's executor
 *
 * A lightweight handle to a GrpcContext. Trivially copyable if it is not tracking outstanding work.
 *
 * Satisfies the [Executor and Networking
 * TS](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/Executor1.html#boost_asio.reference.Executor1.standard_executors)
 * and [Scheduler](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#scheduler) requirements
 * and can therefore be used in places where Asio/libunifex expects an `Executor` or `Scheduler`.
 */
template <class Allocator, std::uint32_t Options>
class BasicGrpcExecutor
    : public detail::ConditionalT<detail::is_outstanding_work_tracked(Options),
                                  detail::GrpcExecutorWorkTrackerBase<Allocator>, detail::GrpcExecutorBase<Allocator>>
{
  private:
    using Base =
        detail::ConditionalT<detail::is_outstanding_work_tracked(Options),
                             detail::GrpcExecutorWorkTrackerBase<Allocator>, detail::GrpcExecutorBase<Allocator>>;

  public:
    /**
     * @brief The associated allocator type
     */
    using allocator_type = Allocator;

    /**
     * @brief Default construct an executor
     *
     * The constructed object may not be used until it is assigned a valid executor, for example through
     * `GrpcContext::get_executor`.
     */
    BasicGrpcExecutor() = default;

    constexpr explicit BasicGrpcExecutor(agrpc::GrpcContext& grpc_context) noexcept(
        std::is_nothrow_default_constructible_v<allocator_type>)
        : BasicGrpcExecutor(grpc_context, allocator_type{})
    {
    }

    constexpr BasicGrpcExecutor(agrpc::GrpcContext& grpc_context, const allocator_type& allocator) noexcept
        : Base(&grpc_context, allocator){}

#if defined(AGRPC_UNIFEX) || defined(AGRPC_STDEXEC) ||                     \
    (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_NO_TS_EXECUTORS)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_NO_TS_EXECUTORS))
          /**
           * @brief Get the underlying GrpcContext
           *
           * Thread-safe
           *
           * Since 1.6.0 this function is hidden when `(BOOST_)ASIO_NO_TS_EXECUTORS` is defined.
           */
          [[nodiscard]] constexpr agrpc::GrpcContext
          &
          context() const noexcept
    {
        return *this->grpc_context();
    }
#endif

    /**
     * @brief Get the associated allocator
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return this->allocator(); }

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
        else if constexpr (detail::IS_EQUALITY_COMPARABLE<Allocator>)
        {
            return lhs.grpc_context() == rhs.grpc_context() && lhs.allocator() == rhs.allocator();
        }
        else
        {
            return lhs.grpc_context() == rhs.grpc_context();
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
        return detail::GrpcContextImplementation::running_in_this_thread(*this->grpc_context());
    }

#if (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_NO_TS_EXECUTORS)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_NO_TS_EXECUTORS))
    /**
     * @brief Signal the GrpcContext that an asynchronous operation is in progress
     *
     * Thread-safe
     *
     * Since 1.6.0 this function is hidden when `(BOOST_)ASIO_NO_TS_EXECUTORS` is defined.
     */
    void on_work_started() const noexcept { this->grpc_context()->work_started(); }

    /**
     * @brief Signal the GrpcContext that an asynchronous operation has completed
     *
     * Once all outstanding asynchronous operations have completed the GrpcContext will go into the stopped state.
     *
     * Thread-safe
     *
     * Since 1.6.0 this function is hidden when `(BOOST_)ASIO_NO_TS_EXECUTORS` is defined.
     */
    void on_work_finished() const noexcept { this->grpc_context()->work_finished(); }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly, it is intended to be used by the
     * [asio::dispatch](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/dispatch.html) free
     * function.
     *
     * Thread-safe
     *
     * Since 1.6.0 this function is hidden when `(BOOST_)ASIO_NO_TS_EXECUTORS` is defined.
     */
    template <class Function, class OtherAllocator>
    void dispatch(Function&& function, const OtherAllocator& other_allocator) const
    {
        detail::create_and_submit_no_arg_operation<false>(
            context(), detail::AllocatorBinder(other_allocator, static_cast<Function&&>(function)));
    }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly, it is intended to be used by the
     * [asio::post](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/post.html) free
     * function.
     *
     * Thread-safe
     *
     * Since 1.6.0 this function is hidden when `(BOOST_)ASIO_NO_TS_EXECUTORS` is defined.
     */
    template <class Function, class OtherAllocator>
    void post(Function&& function, const OtherAllocator& other_allocator) const
    {
        detail::create_and_submit_no_arg_operation<true>(
            context(), detail::AllocatorBinder(other_allocator, static_cast<Function&&>(function)));
    }

    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Do not call this function directly, it is intended to be used by the
     * [asio::defer](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/defer.html) free
     * function.
     *
     * Thread-safe
     *
     * Since 1.6.0 this function is hidden when `(BOOST_)ASIO_NO_TS_EXECUTORS` is defined.
     */
    template <class Function, class OtherAllocator>
    void defer(Function&& function, const OtherAllocator& other_allocator) const
    {
        detail::create_and_submit_no_arg_operation<true>(
            context(), detail::AllocatorBinder(other_allocator, static_cast<Function&&>(function)));
    }
#endif

#if defined(AGRPC_BOOST_ASIO) || defined(AGRPC_STANDALONE_ASIO)
    /**
     * @brief Request the GrpcContext to invoke the given function object
     *
     * Thread-safe
     */
    template <class Function>
    void execute(Function&& function) const
    {
        if constexpr (detail::IS_STD_ALLOCATOR<Allocator>)
        {
            detail::create_and_submit_no_arg_operation<detail::is_blocking_never(Options)>(
                *this->grpc_context(), static_cast<Function&&>(function));
        }
        else
        {
            detail::create_and_submit_no_arg_operation<detail::is_blocking_never(Options)>(
                *this->grpc_context(), detail::AllocatorBinder(this->allocator(), static_cast<Function&&>(function)));
        }
    }
#endif

    /**
     * @brief Create a Sender that completes on the GrpcContext
     *
     * Do not call this function directly. It is intended to be used by the
     * [unifex::schedule](https://github.com/facebookexperimental/libunifex/blob/main/doc/api_reference.md#schedulescheduler-schedule---senderofvoid)
     * customisation point.
     *
     * Thread-safe
     */
    [[nodiscard]] detail::ScheduleSender schedule() const noexcept
    {
        return detail::BasicSenderAccess::create(*this->grpc_context(), detail::ScheduleSenderInitiation{},
                                                 detail::ScheduleSenderImplementation{});
    }

#ifdef AGRPC_STDEXEC
    friend auto tag_invoke(stdexec::schedule_t, const BasicGrpcExecutor& executor) noexcept
    {
        return executor.schedule();
    }

    friend constexpr auto tag_invoke(stdexec::get_forward_progress_guarantee_t, const BasicGrpcExecutor&) noexcept
    {
        return stdexec::forward_progress_guarantee::parallel;
    }
#endif

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    /**
     * @brief Obtain an executor with the blocking.possibly property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::blocking_t::possibly_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_blocking_never(Options, false)>
    {
        return {*this->grpc_context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the blocking.never property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::blocking_t::never_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_blocking_never(Options, true)>
    {
        return {*this->grpc_context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the relationship.fork property
     *
     * The GrpcExecutor always forks.
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::prefer](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/prefer.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto prefer(asio::execution::relationship_t::fork_t) const noexcept { return *this; }

    /**
     * @brief Obtain an executor with the relationship.continuation property
     *
     * The GrpcExecutor does not support continuation.
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::prefer](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/prefer.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto prefer(asio::execution::relationship_t::continuation_t) const noexcept
    {
        return *this;
    }

    /**
     * @brief Obtain an executor with the outstanding_work.tracked property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::outstanding_work_t::tracked_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_outstanding_work_tracked(Options, true)>
    {
        return {*this->grpc_context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the outstanding_work.untracked property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::outstanding_work_t::untracked_t) const noexcept
        -> agrpc::BasicGrpcExecutor<Allocator, detail::set_outstanding_work_tracked(Options, false)>
    {
        return {*this->grpc_context(), this->allocator()};
    }

    /**
     * @brief Obtain an executor with the specified allocator property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    template <class OtherAllocator>
    [[nodiscard]] constexpr auto require(asio::execution::allocator_t<OtherAllocator> other_allocator) const noexcept
        -> agrpc::BasicGrpcExecutor<OtherAllocator, Options>
    {
        return {*this->grpc_context(), other_allocator.value()};
    }

    /**
     * @brief Obtain an executor with the default allocator property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::require](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/require.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr auto require(asio::execution::allocator_t<void>) const noexcept
        -> agrpc::BasicGrpcExecutor<std::allocator<void>, Options>
    {
        return agrpc::BasicGrpcExecutor<std::allocator<void>, Options>{*this->grpc_context()};
    }

    /**
     * @brief Query the current value of the blocking property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr auto query(asio::execution::blocking_t) noexcept
    {
        return typename detail::QueryStaticBlocking<detail::is_blocking_never(Options)>::result_type();
    }

    /**
     * @brief Query the current value of the mapping property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr detail::QueryStaticMapping::result_type query(asio::execution::mapping_t) noexcept
    {
        return {};
    }

    /**
     * @brief Query the current value of the context property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] constexpr agrpc::GrpcContext& query(asio::execution::context_t) const noexcept
    {
        return *this->grpc_context();
    }

    /**
     * @brief Query the current value of the relationship property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr detail::QueryStaticRelationship::result_type query(
        asio::execution::relationship_t) noexcept
    {
        return {};
    }

    /**
     * @brief Query the current value of the outstanding_work property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/query.html) customisation
     * point.
     *
     * Thread-safe
     */
    [[nodiscard]] static constexpr auto query(asio::execution::outstanding_work_t) noexcept
    {
        return typename detail::QueryStaticWorkTracked<detail::is_outstanding_work_tracked(Options)>::result_type();
    }

    /**
     * @brief Query the current value of the allocator property
     *
     * Do not call this function directly. It is intended to be used by the
     * [asio::query](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/query.html) customisation
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
 * The default GrpcExecutor does not track outstanding work, has the
 * [relationship.fork](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/execution__relationship_t__fork_t.html)
 * and
 * [blocking.never](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/execution__blocking_t__never_t.html)
 * properties and uses the default allocator (`std::allocator<void>`).
 */
using GrpcExecutor = agrpc::BasicGrpcExecutor<>;

AGRPC_NAMESPACE_END

template <class Allocator, std::uint32_t Options, class Alloc>
struct std::uses_allocator<agrpc::BasicGrpcExecutor<Allocator, Options>, Alloc> : std::false_type
{
};

#if (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT))
template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::equality_comparable<agrpc::BasicGrpcExecutor<Allocator, Options>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;
};
#endif

#if (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT))
template <class Allocator, std::uint32_t Options, class F>
struct agrpc::asio::traits::execute_member<agrpc::BasicGrpcExecutor<Allocator, Options>, F>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
};
#endif

#if (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT))
template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::require_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                           agrpc::asio::execution::blocking_t::possibly_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::BasicGrpcExecutor<Allocator, agrpc::detail::set_blocking_never(Options, false)>;
};

template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::require_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                           agrpc::asio::execution::blocking_t::never_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::BasicGrpcExecutor<Allocator, agrpc::detail::set_blocking_never(Options, true)>;
};

template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::require_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                           agrpc::asio::execution::outstanding_work_t::tracked_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::BasicGrpcExecutor<Allocator, agrpc::detail::set_outstanding_work_tracked(Options, true)>;
};

template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::require_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                           agrpc::asio::execution::outstanding_work_t::untracked_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type =
        agrpc::BasicGrpcExecutor<Allocator, agrpc::detail::set_outstanding_work_tracked(Options, false)>;
};

template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::require_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                           agrpc::asio::execution::allocator_t<void>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::BasicGrpcExecutor<std::allocator<void>, Options>;
};

template <class Allocator, std::uint32_t Options, typename OtherAllocator>
struct agrpc::asio::traits::require_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                           agrpc::asio::execution::allocator_t<OtherAllocator>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::BasicGrpcExecutor<OtherAllocator, Options>;
};
#endif

#if (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT))
template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::prefer_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                          agrpc::asio::execution::relationship_t::fork_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::BasicGrpcExecutor<Allocator, Options>;
};

template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::prefer_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                          agrpc::asio::execution::relationship_t::continuation_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    // Relationship continuation is not supported
    using result_type = agrpc::BasicGrpcExecutor<Allocator, Options>;
};
#endif

#if defined(AGRPC_BOOST_ASIO) || defined(AGRPC_STANDALONE_ASIO)
template <class Allocator, std::uint32_t Options, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::BasicGrpcExecutor<Allocator, Options>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::blocking_t>>>
    : agrpc::detail::QueryStaticBlocking<agrpc::detail::is_blocking_never(Options)>
{
};

template <class Allocator, std::uint32_t Options, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::BasicGrpcExecutor<Allocator, Options>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::relationship_t>>>
    : agrpc::detail::QueryStaticRelationship
{
};

template <class Allocator, std::uint32_t Options, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::BasicGrpcExecutor<Allocator, Options>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::outstanding_work_t>>>
    : agrpc::detail::QueryStaticWorkTracked<agrpc::detail::is_outstanding_work_tracked(Options)>
{
};

template <class Allocator, std::uint32_t Options, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::BasicGrpcExecutor<Allocator, Options>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::mapping_t>>>
    : agrpc::detail::QueryStaticMapping
{
};
#endif

#if (defined(AGRPC_BOOST_ASIO) && !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)) || \
    (defined(AGRPC_STANDALONE_ASIO) && !defined(ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT))
template <class Allocator, std::uint32_t Options>
struct agrpc::asio::traits::query_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                         agrpc::asio::execution::context_t>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::GrpcContext&;
};

template <class Allocator, std::uint32_t Options, class OtherAllocator>
struct agrpc::asio::traits::query_member<agrpc::BasicGrpcExecutor<Allocator, Options>,
                                         agrpc::asio::execution::allocator_t<OtherAllocator>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = Allocator;
};
#endif

#include <agrpc/detail/grpc_context_definition.hpp>

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_GRPC_EXECUTOR_HPP
