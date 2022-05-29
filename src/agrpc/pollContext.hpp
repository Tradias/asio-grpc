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
#include <agrpc/detail/oneShotAllocator.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpcContext.hpp>

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/steady_timer.hpp>
#else
#include <boost/asio/steady_timer.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Default PollContext traits
 */
struct DefaultPollContextTraits
{
    /**
     * @brief The default buffer size in bytes
     */
    static constexpr std::size_t BUFFER_SIZE{200};

    /**
     * @brief The desired maximum latency
     *
     * The maximum latency when going from an idle GrpcContext to a busy one. A low latency leads to higher CPU
     * consumption during idle time.
     */
    static constexpr std::chrono::nanoseconds MAX_LATENCY{200};
};

/**
 * @brief (experimental) Helper class to run a GrpcContext in a different execution context
 *
 * Example showing how to share a thread with an `asio::io_context`:
 *
 * @snippet client.cpp poll_context-with-io_context
 *
 * @tparam Executor The executor type
 * @tparam Traits The traits type, defaults to `agrpc::DefaultPollContextTraits`. If the static assertion
 * 'OneShotAllocator has insufficient capacity' fails then create your own traits to increase the buffer size of
 * the PollContext. Example:
 * @code{cpp}
 * struct MyTraits
 * {
 *   static constexpr std::size_t BUFFER_SIZE{256};
 * }
 * @endcode
 * Before asio-grpc 1.6.0 custom traits had to inherit from the default:
 * `struct MyTraits : agrpc::DefaultPollContextTraits`.
 *
 * @since 1.5.0
 */
template <class Executor, class Traits = agrpc::DefaultPollContextTraits>
class PollContext
{
  private:
    using ResolvedTraits = detail::ResolvedPollContextTraits<Traits>;

    static constexpr auto BUFFER_SIZE = ResolvedTraits::BUFFER_SIZE;

  public:
    /**
     * @brief Construct a PollContext from an Executor
     */
    template <class Exec>
    explicit PollContext(Exec&& executor)
        : timer(executor),
          executor(asio::prefer(asio::require(std::forward<Exec>(executor), asio::execution::blocking_t::never),
                                asio::execution::relationship_t::continuation,
                                asio::execution::allocator(this->allocator())))

    {
    }

    PollContext(const PollContext&) = delete;
    PollContext(PollContext&&) = delete;
    PollContext& operator=(const PollContext&) = delete;
    PollContext& operator=(PollContext&&) = delete;

    /**
     * @brief Repeatedly call .poll() on the GrpcContext until it is stopped
     *
     * Calls grpc_context.poll() in the execution context of the PollContext's executor.
     */
    void async_poll(agrpc::GrpcContext& grpc_context);

    /**
     * @brief Repeatedly call .poll_completion_queue() on the GrpcContext until it is stopped
     *
     * Calls grpc_context.poll_completion_queue() in the execution context of the PollContext's executor.
     */
    void async_poll_completion_queue(agrpc::GrpcContext& grpc_context);

    /**
     * @brief Repeatedly call .poll() on the GrpcContext until the provided StopPredicate returns true
     *
     * @param stop_predicate A function that returns true when the polling should stop. Its signature should be
     * `bool(agrpc::GrpcContext&)`.
     */
    template <class StopPredicate>
    void async_poll(agrpc::GrpcContext& grpc_context, StopPredicate stop_predicate);

    /**
     * @brief Repeatedly call .poll_completion_queue() on the GrpcContext until the provided StopPredicate returns true
     *
     * @param stop_predicate A function that returns true when the polling should stop. Its signature should be
     * `bool(agrpc::GrpcContext&)`.
     */
    template <class StopPredicate>
    void async_poll_completion_queue(agrpc::GrpcContext& grpc_context, StopPredicate stop_predicate);

  private:
    template <class>
    friend struct detail::PollContextHandler;

    using Backoff =
        detail::Backoff<std::chrono::duration_cast<detail::BackoffDelay>(ResolvedTraits::MAX_LATENCY).count()>;
    using Allocator = detail::OneShotAllocator<std::byte, BUFFER_SIZE>;
    using Exec = decltype(asio::prefer(asio::require(std::declval<Executor>(), asio::execution::blocking_t::never),
                                       asio::execution::relationship_t::continuation,
                                       asio::execution::allocator(Allocator{nullptr})));

    auto allocator() noexcept { return Allocator{&buffer}; }

    std::aligned_storage_t<BUFFER_SIZE> buffer;
    asio::basic_waitable_timer<std::chrono::steady_clock, asio::wait_traits<std::chrono::steady_clock>, Executor> timer;
    Exec executor;
    Backoff backoff;
};

template <class Executor>
PollContext(Executor&&) -> PollContext<detail::RemoveCvrefT<Executor>>;

// Implementation details
namespace detail
{
template <class Traits, class = void>
inline constexpr auto RESOLVED_POLL_CONTEXT_BUFFER_SIZE = agrpc::DefaultPollContextTraits::BUFFER_SIZE;

template <class Traits>
inline constexpr auto RESOLVED_POLL_CONTEXT_BUFFER_SIZE<Traits, decltype((void)Traits::BUFFER_SIZE)> =
    Traits::BUFFER_SIZE;

template <class Traits, class = void>
inline constexpr auto RESOLVED_POLL_CONTEXT_MAX_LATENCY = agrpc::DefaultPollContextTraits::MAX_LATENCY;

template <class Traits>
inline constexpr auto RESOLVED_POLL_CONTEXT_MAX_LATENCY<Traits, decltype((void)Traits::MAX_LATENCY)> =
    Traits::MAX_LATENCY;

template <class Traits>
struct ResolvedPollContextTraits
{
    static constexpr auto BUFFER_SIZE = detail::RESOLVED_POLL_CONTEXT_BUFFER_SIZE<Traits>;
    static constexpr auto MAX_LATENCY = detail::RESOLVED_POLL_CONTEXT_MAX_LATENCY<Traits>;
};

struct IsGrpcContextStoppedPredicate
{
    bool operator()(const agrpc::GrpcContext& grpc_context) const noexcept { return grpc_context.is_stopped(); }
};

template <class Poller>
struct PollContextHandler
{
    using ResolvedTraits = detail::ResolvedPollContextTraits<typename Poller::PollContextTraits>;
    using PollContext = typename Poller::PollContext;
    using StopPredicate = typename Poller::StopPredicate;
    using allocator_type = typename PollContext::Allocator;

    agrpc::GrpcContext& grpc_context;
    PollContext& poll_context;
    StopPredicate stop_predicate;

    void operator()(detail::ErrorCode = {})
    {
        if constexpr (detail::BackoffDelay::zero() == ResolvedTraits::MAX_LATENCY)
        {
            Poller::poll(grpc_context);
            Poller::async_poll(poll_context, grpc_context, std::move(stop_predicate));
        }
        else
        {
            if (Poller::poll(grpc_context))
            {
                poll_context.backoff.reset();
                Poller::async_poll(poll_context, grpc_context, std::move(stop_predicate));
                return;
            }
            const auto delay = poll_context.backoff.next();
            if (detail::BackoffDelay::zero() == delay)
            {
                Poller::async_poll(poll_context, grpc_context, std::move(stop_predicate));
                return;
            }
            if (stop_predicate(grpc_context))
            {
                return;
            }
            poll_context.timer.expires_after(delay);
            poll_context.timer.async_wait(std::move(*this));
        }
    }

    allocator_type get_allocator() const noexcept { return poll_context.allocator(); }
};

template <class Executor, class Traits, class StopPred>
struct RegularPoller
{
    using PollContextTraits = Traits;
    using PollContext = agrpc::PollContext<Executor, Traits>;
    using StopPredicate = StopPred;

    static bool poll(agrpc::GrpcContext& grpc_context) { return grpc_context.poll(); }

    static void async_poll(PollContext& poll_context, agrpc::GrpcContext& grpc_context, StopPredicate stop_predicate)
    {
        poll_context.async_poll(grpc_context, std::move(stop_predicate));
    }
};

template <class Executor, class Traits, class StopPred>
struct CompletionQueuePoller
{
    using PollContextTraits = Traits;
    using PollContext = agrpc::PollContext<Executor, Traits>;
    using StopPredicate = StopPred;

    static bool poll(agrpc::GrpcContext& grpc_context) { return grpc_context.poll_completion_queue(); }

    static void async_poll(PollContext& poll_context, agrpc::GrpcContext& grpc_context, StopPredicate stop_predicate)
    {
        poll_context.async_poll_completion_queue(grpc_context, std::move(stop_predicate));
    }
};
}

template <class Executor, class Traits>
void PollContext<Executor, Traits>::async_poll(agrpc::GrpcContext& grpc_context)
{
    this->async_poll(grpc_context, detail::IsGrpcContextStoppedPredicate{});
}

template <class Executor, class Traits>
void PollContext<Executor, Traits>::async_poll_completion_queue(agrpc::GrpcContext& grpc_context)
{
    this->async_poll_completion_queue(grpc_context, detail::IsGrpcContextStoppedPredicate{});
}

template <class Executor, class Traits>
template <class StopPredicate>
void PollContext<Executor, Traits>::async_poll(agrpc::GrpcContext& grpc_context, StopPredicate stop_predicate)
{
    if (stop_predicate(grpc_context))
    {
        return;
    }
    asio::execution::execute(executor,
                             detail::PollContextHandler<detail::RegularPoller<Executor, Traits, StopPredicate>>{
                                 grpc_context, *this, std::move(stop_predicate)});
}

template <class Executor, class Traits>
template <class StopPredicate>
void PollContext<Executor, Traits>::async_poll_completion_queue(agrpc::GrpcContext& grpc_context,
                                                                StopPredicate stop_predicate)
{
    if (stop_predicate(grpc_context))
    {
        return;
    }
    asio::execution::execute(executor,
                             detail::PollContextHandler<detail::CompletionQueuePoller<Executor, Traits, StopPredicate>>{
                                 grpc_context, *this, std::move(stop_predicate)});
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_POLLCONTEXT_HPP
