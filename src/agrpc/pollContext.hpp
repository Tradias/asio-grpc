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

#ifndef AGRPC_AGRPC_POLLCONTEXT_HPP
#define AGRPC_AGRPC_POLLCONTEXT_HPP

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/backoff.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpcContextImplementation.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpcContext.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Default PollContext traits
 */
struct DefaultPollContextTraits
{
    /**
     * @brief The desired maximum latency
     *
     * The maximum latency when going from an idle GrpcContext to a busy one. A low latency leads to higher CPU
     * consumption during idle time.
     */
    static constexpr std::chrono::microseconds MAX_LATENCY{250};

    /**
     * @brief How to poll the execution context
     *
     * This function should let the execution context process some work without blocking.
     */
    template <class ExecutionContext>
    static bool poll(ExecutionContext& execution_context)
    {
        return 0 != execution_context.poll();
    }
};

// Implementation details
namespace detail
{
template <class Traits, class = void>
inline constexpr auto RESOLVED_POLL_CONTEXT_MAX_LATENCY = agrpc::DefaultPollContextTraits::MAX_LATENCY;

template <class Traits>
inline constexpr auto RESOLVED_POLL_CONTEXT_MAX_LATENCY<Traits, decltype((void)Traits::MAX_LATENCY)> =
    Traits::MAX_LATENCY;

template <class Traits, class ExecutionContext, class = void>
inline constexpr auto RESOLVED_POLL_CONTEXT_HAS_POLL = false;

template <class Traits, class ExecutionContext>
inline constexpr auto RESOLVED_POLL_CONTEXT_HAS_POLL<Traits, ExecutionContext,
                                                     decltype((void)Traits::poll(std::declval<ExecutionContext&>()))> =
    true;

template <class Traits>
struct ResolvedPollContextTraits
{
    static constexpr auto MAX_LATENCY = detail::RESOLVED_POLL_CONTEXT_MAX_LATENCY<Traits>;

    template <class ExecutionContext>
    static bool poll(ExecutionContext& execution_context)
    {
        if constexpr (detail::RESOLVED_POLL_CONTEXT_HAS_POLL<Traits, ExecutionContext>)
        {
            return Traits::poll(execution_context);
        }
        else
        {
            return agrpc::DefaultPollContextTraits::poll(execution_context);
        }
    }
};

struct IsExecutionContextStoppedPredicate
{
    template <class ExecutionContext>
    bool operator()(const ExecutionContext& execution_context) const noexcept
    {
        return execution_context.stopped();
    }
};

template <class Traits>
struct RunFn
{
    template <class ExecutionContext, class StopPredicate = detail::IsExecutionContextStoppedPredicate>
    void operator()(agrpc::GrpcContext& grpc_context, ExecutionContext& execution_context,
                    StopPredicate stop_predicate = {}) const
    {
        using ResolvedTraits = detail::ResolvedPollContextTraits<Traits>;
        using Backoff =
            detail::Backoff<std::chrono::duration_cast<detail::BackoffDelay>(ResolvedTraits::MAX_LATENCY).count()>;
        Backoff backoff;
        [[maybe_unused]] detail::GrpcContextThreadContext thread_context;
        detail::ThreadLocalGrpcContextGuard guard{grpc_context};
        const auto loop_function = [&]()
        {
            if (stop_predicate(execution_context))
            {
                return false;
            }
            auto delay = backoff.next();
            if (ResolvedTraits::poll(execution_context))
            {
                delay = backoff.reset();
            }
            while (detail::BackoffDelay::zero() == delay)
            {
                detail::GrpcContextImplementation::do_one(grpc_context, detail::GrpcContextImplementation::TIME_ZERO);
                if (stop_predicate(execution_context))
                {
                    return false;
                }
                if (ResolvedTraits::poll(execution_context))
                {
                    delay = backoff.reset();
                }
                else
                {
                    delay = backoff.next();
                }
            }
            const auto delay_timespec = ::gpr_time_from_nanos(
                std::chrono::duration_cast<std::chrono::nanoseconds>(delay).count(), GPR_TIMESPAN);
            auto timespec = ::gpr_now(GPR_CLOCK_MONOTONIC);
            timespec = ::gpr_time_add(timespec, delay_timespec);
            detail::GrpcContextImplementation::do_one(grpc_context, timespec);
            return true;
        };
        while (loop_function())
        {
            //
        }
    }
};
}

template <class Traits = agrpc::DefaultPollContextTraits>
inline constexpr detail::RunFn<Traits> run{};

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_POLLCONTEXT_HPP
