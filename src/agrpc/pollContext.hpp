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

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/oneShotAllocator.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Default PollContext traits
 */
struct DefaultPollContextTraits
{
    /**
     * @brief The default buffer size
     */
    static constexpr std::size_t BUFFER_SIZE = 96;
};

template <class Executor, class Traits = agrpc::DefaultPollContextTraits>
class PollContext;

namespace detail
{
struct IsGrpcContextStoppedPredicate
{
    bool operator()(agrpc::GrpcContext& grpc_context) const noexcept { return grpc_context.is_stopped(); }
};

template <class Executor, class Traits, class StopPredicate>
struct PollContextHandler
{
    agrpc::GrpcContext& grpc_context;
    agrpc::PollContext<Executor, Traits>& poll_context;
    StopPredicate stop_predicate;

    void operator()()
    {
        grpc_context.poll();
        poll_context.async_poll(grpc_context, std::move(stop_predicate));
    }
};
}

/**
 * @brief (experimental) Helper class to run a GrpcContext in a different execution context
 *
 * Example showing how to share a thread with an `asio::io_context`:
 *
 * @snippet client.cpp poll_context-with-io_context
 *
 * @tparam Executor The executor type
 * @tparam Traits The traits type, defaults to `agrpc::DefaultPollContextTraits`. If the static assertion
 * 'OneShotAllocator has insufficient capacity' triggers then inherit from the default to increase the buffer size of
 * the PollContext. Example:
 * @code{cpp}
 * struct MyTraits : agrpc::DefaultPollContextTraits
 * {
 *   static constexpr std::size_t BUFFER_SIZE = 128;
 * }
 * @endcode
 *
 * @since 1.5.0
 */
template <class Executor, class Traits>
class PollContext
{
  private:
    static constexpr auto BUFFER_SIZE = Traits::BUFFER_SIZE;

  public:
    /**
     * @brief Construct a PollContext from an Executor
     */
    template <class Exec>
    explicit PollContext(Exec&& executor)
        : executor(asio::prefer(asio::require(std::forward<Exec>(executor), asio::execution::blocking.never),
                                asio::execution::relationship.continuation,
                                asio::execution::allocator(detail::OneShotAllocator<std::byte, BUFFER_SIZE>{&buffer})))
    {
    }

    PollContext(const PollContext&) = delete;
    PollContext(PollContext&&) = delete;
    PollContext& operator=(const PollContext&) = delete;
    PollContext& operator=(PollContext&&) = delete;

    /**
     * @brief Repeatedly call .poll() on the GrpcContext until it is stopped
     */
    void async_poll(agrpc::GrpcContext& grpc_context)
    {
        this->async_poll(grpc_context, detail::IsGrpcContextStoppedPredicate{});
    }

    /**
     * @brief Repeatedly call .poll() on the GrpcContext until the provided StopPredicate returns true
     *
     * @param stop_predicate A function that returns true when the polling should stop. Its signature should be
     * `bool(agrpc::GrpcContext&)`.
     */
    template <class StopPredicate>
    void async_poll(agrpc::GrpcContext& grpc_context, StopPredicate stop_predicate)
    {
        if (stop_predicate(grpc_context))
        {
            return;
        }
        asio::execution::execute(executor, detail::PollContextHandler<Executor, Traits, StopPredicate>{
                                               grpc_context, *this, std::move(stop_predicate)});
    }

  private:
    using Exec =
        decltype(asio::prefer(asio::require(std::declval<Executor>(), asio::execution::blocking_t::never),
                              asio::execution::relationship_t::continuation,
                              asio::execution::allocator(detail::OneShotAllocator<std::byte, BUFFER_SIZE>{nullptr})));

    Exec executor;
    std::aligned_storage_t<BUFFER_SIZE> buffer;
};

template <class Executor>
PollContext(Executor&&) -> PollContext<detail::RemoveCvrefT<Executor>>;

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_POLLCONTEXT_HPP
