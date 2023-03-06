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

#ifndef AGRPC_AGRPC_RUN_HPP
#define AGRPC_AGRPC_RUN_HPP

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/backoff.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/grpc_context.hpp>

#include <chrono>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Default run traits
 *
 * @since 1.7.0
 */
struct DefaultRunTraits
{
    /**
     * @brief The desired maximum latency
     *
     * The maximum latency between consecutive polls of the execution context.
     */
    static constexpr std::chrono::microseconds MAX_LATENCY{250};

    /**
     * @brief How to poll the execution context
     *
     * This function should let the execution context process some work without sleeping and return true if any work has
     * been processed.
     */
    template <class ExecutionContext>
    static bool poll(ExecutionContext& execution_context)
    {
        return 0 != execution_context.poll();
    }

    template <class ExecutionContext, class Rep, class Period>
    static bool run_for(ExecutionContext& execution_context, std::chrono::duration<Rep, Period> duration)
    {
        return 0 != execution_context.run_for(duration);
    }

    template <class ExecutionContext>
    static bool is_stopped(ExecutionContext& execution_context)
    {
        return execution_context.stopped();
    }
};

/**
 * @brief (experimental) Run an execution context in the same thread as a GrpcContext
 *
 * The GrpcContext should be in the ready state when this function is invoked, other than that semantically identical to
 * GrpcContext::run(). This function ends when the GrpcContext is stopped, e.g. because it ran out of work.
 *
 * @tparam Traits See DefaultRunTraits
 *
 * @since 1.7.0
 */
template <class Traits = agrpc::DefaultRunTraits, class ExecutionContext = void>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context);

/**
 * @brief (experimental) Run an execution context in the same thread as a GrpcContext
 *
 * The GrpcContext should be in the ready state when this function is invoked, other than that semantically identical to
 * GrpcContext::run(). This function ends when the `stop_condition` returns `true`.
 *
 * @tparam Traits See DefaultRunTraits
 *
 * @since 1.7.0
 */
template <class Traits = agrpc::DefaultRunTraits, class ExecutionContext = void, class StopCondition = void>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context, StopCondition stop_condition);

/**
 * @brief (experimental) Run an execution context in the same thread as a GrpcContext's completion queue
 *
 * The GrpcContext should be in the ready state when this function is invoked, other than that semantically identical to
 * GrpcContext::run_completion_queue(). This function ends when the GrpcContext is stopped, e.g. because it ran out of
 * work.
 *
 * @tparam Traits See DefaultRunTraits
 *
 * @since 2.0.0
 */
template <class Traits = agrpc::DefaultRunTraits, class ExecutionContext = void>
void run_completion_queue(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context);

/**
 * @brief (experimental) Run an execution context in the same thread as a GrpcContext's completion queue
 *
 * The GrpcContext should be in the ready state when this function is invoked, other than that semantically identical to
 * GrpcContext::run_completion_queue(). This function ends when the `stop_condition` returns `true`.
 *
 * @tparam Traits See DefaultRunTraits
 *
 * @since 2.0.0
 */
template <class Traits = agrpc::DefaultRunTraits, class ExecutionContext = void, class StopCondition = void>
void run_completion_queue(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context,
                          StopCondition stop_condition);

// Implementation details
namespace detail
{
template <class Traits, class = void>
inline constexpr auto RESOLVED_RUN_TRAITS_MAX_LATENCY = agrpc::DefaultRunTraits::MAX_LATENCY;

template <class Traits>
inline constexpr auto RESOLVED_RUN_TRAITS_MAX_LATENCY<Traits, decltype((void)Traits::MAX_LATENCY)> =
    Traits::MAX_LATENCY;

template <class Traits, class ExecutionContext, class = void>
inline constexpr auto RESOLVED_RUN_TRAITS_HAS_POLL = false;

template <class Traits, class ExecutionContext>
inline constexpr auto RESOLVED_RUN_TRAITS_HAS_POLL<Traits, ExecutionContext,
                                                   decltype((void)Traits::poll(std::declval<ExecutionContext&>()))> =
    true;

template <class Traits, class ExecutionContext, class = void>
inline constexpr auto RESOLVED_RUN_TRAITS_HAS_RUN_FOR = false;

template <class Traits, class ExecutionContext>
inline constexpr auto RESOLVED_RUN_TRAITS_HAS_RUN_FOR<
    Traits, ExecutionContext,
    decltype((void)Traits::run_for(std::declval<ExecutionContext&>(), std::declval<detail::BackoffDelay>()))> = true;

template <class Traits, class ExecutionContext, class = void>
inline constexpr auto RESOLVED_RUN_TRAITS_HAS_IS_STOPPED = false;

template <class Traits, class ExecutionContext>
inline constexpr auto RESOLVED_RUN_TRAITS_HAS_IS_STOPPED<
    Traits, ExecutionContext, decltype((void)Traits::is_stopped(std::declval<ExecutionContext&>()))> = true;

template <class Traits>
struct ResolvedRunTraits
{
    static constexpr auto MAX_LATENCY = detail::RESOLVED_RUN_TRAITS_MAX_LATENCY<Traits>;

    template <class ExecutionContext>
    static bool poll(ExecutionContext& execution_context)
    {
        if constexpr (detail::RESOLVED_RUN_TRAITS_HAS_POLL<Traits, ExecutionContext>)
        {
            return Traits::poll(execution_context);
        }
        else
        {
            return agrpc::DefaultRunTraits::poll(execution_context);
        }
    }

    template <class ExecutionContext, class Rep, class Period>
    static bool run_for(ExecutionContext& execution_context, std::chrono::duration<Rep, Period> duration)
    {
        if constexpr (detail::RESOLVED_RUN_TRAITS_HAS_RUN_FOR<Traits, ExecutionContext>)
        {
            return Traits::run_for(execution_context, duration);
        }
        else
        {
            return agrpc::DefaultRunTraits::run_for(execution_context, duration);
        }
    }

    template <class ExecutionContext>
    static bool is_stopped(ExecutionContext& execution_context)
    {
        if constexpr (detail::RESOLVED_RUN_TRAITS_HAS_IS_STOPPED<Traits, ExecutionContext>)
        {
            return Traits::is_stopped(execution_context);
        }
        else
        {
            return agrpc::DefaultRunTraits::is_stopped(execution_context);
        }
    }
};

struct AlwaysFalseCondition
{
    [[nodiscard]] constexpr bool operator()() const noexcept { return false; }
};

struct GrpcContextDoOne
{
    static bool poll(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline)
    {
        return detail::GrpcContextImplementation::do_one(grpc_context, deadline);
    }
};

struct GrpcContextDoOneCompletionQueue
{
    static bool poll(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline)
    {
        return detail::GrpcContextImplementation::do_one_completion_queue(grpc_context, deadline);
    }
};

struct IsGrpcContextStopped
{
    bool is_stopped_{};

    bool operator()(agrpc::GrpcContext& grpc_context)
    {
        is_stopped_ = grpc_context.is_stopped();
        return is_stopped_;
    }

    constexpr explicit operator bool() const noexcept { return is_stopped_; }
};

template <class GrpcContextPoller, class Traits, class ExecutionContext, class StopCondition>
void run_impl(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context, StopCondition stop_condition)
{
    using ResolvedTraits = detail::ResolvedRunTraits<Traits>;
    using Backoff =
        detail::Backoff<std::chrono::duration_cast<detail::BackoffDelay>(ResolvedTraits::MAX_LATENCY).count()>;
    [[maybe_unused]] detail::GrpcContextThreadContext thread_context;
    detail::ThreadLocalGrpcContextGuard guard{grpc_context};
    Backoff backoff;
    auto delay = backoff.next();
    IsGrpcContextStopped is_grpc_context_stopped{};
    while (!stop_condition() &&
           (!is_grpc_context_stopped(grpc_context) || !ResolvedTraits::is_stopped(execution_context)))
    {
        const bool has_polled = [&]
        {
            if (is_grpc_context_stopped)
            {
                return ResolvedTraits::run_for(execution_context, delay);
            }
            return ResolvedTraits::poll(execution_context);
        }();
        if (!is_grpc_context_stopped)
        {
            const auto delay_timespec = detail::BackoffDelay::zero() == delay
                                            ? detail::GrpcContextImplementation::TIME_ZERO
                                            : detail::gpr_timespec_from_now(delay);
            GrpcContextPoller::poll(grpc_context, delay_timespec);
        }
        if (has_polled)
        {
            delay = backoff.reset();
        }
        else
        {
            delay = backoff.next();
        }
    }
}
}

template <class Traits, class ExecutionContext>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context)
{
    agrpc::run<Traits>(grpc_context, execution_context, detail::AlwaysFalseCondition{});
}

template <class Traits, class ExecutionContext, class StopCondition>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context, StopCondition stop_condition)
{
    detail::run_impl<detail::GrpcContextDoOne, Traits>(grpc_context, execution_context,
                                                       static_cast<StopCondition&&>(stop_condition));
}

template <class Traits, class ExecutionContext>
void run_completion_queue(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context)
{
    agrpc::run_completion_queue<Traits>(grpc_context, execution_context, detail::AlwaysFalseCondition{});
}

template <class Traits, class ExecutionContext, class StopCondition>
void run_completion_queue(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context,
                          StopCondition stop_condition)
{
    detail::run_impl<detail::GrpcContextDoOneCompletionQueue, Traits>(grpc_context, execution_context,
                                                                      static_cast<StopCondition&&>(stop_condition));
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_RUN_HPP
