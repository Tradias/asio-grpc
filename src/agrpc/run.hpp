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

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/backoff.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpcContextImplementation.hpp>
#include <agrpc/grpcContext.hpp>

#include <chrono>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Default run traits
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
     * This function should let the execution context process some work without sleeping.
     */
    template <class ExecutionContext>
    static bool poll(ExecutionContext& execution_context)
    {
        return 0 != execution_context.poll();
    }
};

/**
 * @brief (experimental) Run an execution context in the same thread as a GrpcContext
 *
 * The GrpcContext should be in the ready state when this function is invoked, other than that semantically identical to
 * GrpcContext::run(). This function ends when both contexts are stopped.
 *
 * @tparam Traits See DefaultRunTraits
 */
template <class Traits = agrpc::DefaultRunTraits, class ExecutionContext = void>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context);

/**
 * @brief (experimental) Run an execution context in the same thread as a GrpcContext
 *
 * The GrpcContext should be in the ready state when this function is invoked, other than that semantically identical to
 * GrpcContext::run(). This function ends when the `stop_condition` returns `false`.
 *
 * @tparam Traits See DefaultRunTraits
 */
template <class Traits = agrpc::DefaultRunTraits, class ExecutionContext = void, class StopCondition = void>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context, StopCondition stop_condition);

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
};

template <class ExecutionContext>
struct AreContextsStoppedCondition
{
    const agrpc::GrpcContext& grpc_context;
    ExecutionContext& execution_context;

    [[nodiscard]] bool operator()() const noexcept { return grpc_context.is_stopped() && execution_context.stopped(); }
};
}

template <class Traits, class ExecutionContext>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context)
{
    agrpc::run<Traits>(grpc_context, execution_context,
                       detail::AreContextsStoppedCondition<ExecutionContext>{grpc_context, execution_context});
}

template <class Traits, class ExecutionContext, class StopCondition>
void run(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context, StopCondition stop_condition)
{
    using ResolvedTraits = detail::ResolvedRunTraits<Traits>;
    using Backoff =
        detail::Backoff<std::chrono::duration_cast<detail::BackoffDelay>(ResolvedTraits::MAX_LATENCY).count()>;
    [[maybe_unused]] detail::GrpcContextThreadContext thread_context;
    detail::ThreadLocalGrpcContextGuard guard{grpc_context};
    Backoff backoff;
    auto delay = backoff.next();
    while (!stop_condition())
    {
        const auto has_polled = ResolvedTraits::poll(execution_context);
        const auto delay_timespec = detail::BackoffDelay::zero() == delay ? detail::GrpcContextImplementation::TIME_ZERO
                                                                          : detail::gpr_timespec_from_now(delay);
        detail::GrpcContextImplementation::do_one(grpc_context, delay_timespec);
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

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_RUN_HPP
