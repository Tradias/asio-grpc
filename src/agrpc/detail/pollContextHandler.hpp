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

#ifndef AGRPC_DETAIL_POLLCONTEXTHANDLER_HPP
#define AGRPC_DETAIL_POLLCONTEXTHANDLER_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/backoff.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/oneShotAllocator.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

template <class Executor, class Traits = agrpc::DefaultPollContextTraits>
class PollContext;

namespace detail
{
struct IsGrpcContextStoppedPredicate
{
    bool operator()(const agrpc::GrpcContext& grpc_context) const noexcept { return grpc_context.is_stopped(); }
};

template <class Executor, class Traits, class StopPredicate>
struct PollContextHandler
{
    using PollContext = agrpc::PollContext<Executor, Traits>;
    using allocator_type = typename PollContext::Allocator;

    agrpc::GrpcContext& grpc_context;
    agrpc::PollContext<Executor, Traits>& poll_context;
    StopPredicate stop_predicate;

    void operator()(detail::ErrorCode = {})
    {
        if constexpr (detail::BackoffDelay::zero() == Traits::MAX_LATENCY)
        {
            poll_context.async_poll(grpc_context, std::move(stop_predicate));
        }
        else
        {
            if (grpc_context.poll())
            {
                poll_context.backoff.reset();
                poll_context.async_poll(grpc_context, std::move(stop_predicate));
            }
            else
            {
                const auto delay = poll_context.backoff.next();
                if (detail::BackoffDelay::zero() == delay)
                {
                    poll_context.async_poll(grpc_context, std::move(stop_predicate));
                }
                else
                {
                    poll_context.timer.expires_after(delay);
                    poll_context.timer.async_wait(std::move(*this));
                }
            }
        }
    }

    allocator_type get_allocator() const noexcept { return poll_context.allocator(); }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POLLCONTEXTHANDLER_HPP
