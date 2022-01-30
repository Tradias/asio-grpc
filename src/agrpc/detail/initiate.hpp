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

#ifndef AGRPC_DETAIL_INITIATE_HPP
#define AGRPC_DETAIL_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/associatedCompletionHandler.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/grpcContextInteraction.hpp"
#include "agrpc/detail/grpcSender.hpp"
#include "agrpc/detail/grpcSubmit.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct UseSender
{
    agrpc::GrpcContext& grpc_context;
};

template <class Executor>
decltype(auto) query_grpc_context(const Executor& executor)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (asio::can_query_v<Executor, asio::execution::context_t>)
    {
        return static_cast<agrpc::GrpcContext&>(asio::query(executor, asio::execution::context));
    }
    else
#endif
    {
        return static_cast<agrpc::GrpcContext&>(executor.context());
    }
}

template <class InitiatingFunction, class StopFunction = detail::Empty>
struct GrpcInitiator
{
    InitiatingFunction initiating_function;

    explicit GrpcInitiator(InitiatingFunction initiating_function) : initiating_function(std::move(initiating_function))
    {
    }

    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler)
    {
        const auto [executor, allocator] = detail::get_associated_executor_and_allocator(completion_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        if AGRPC_UNLIKELY (grpc_context.is_stopped())
        {
            return;
        }
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if constexpr (!std::is_same_v<detail::Empty, StopFunction>)
        {
            if (auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
                cancellation_slot.is_connected())
            {
                cancellation_slot.template emplace<StopFunction>(initiating_function);
            }
        }
#endif
        detail::grpc_submit(grpc_context, std::move(this->initiating_function), std::move(completion_handler),
                            allocator);
    }
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class CompletionHandler, class Payload>
struct GrpcCompletionHandlerWithPayload : detail::AssociatedCompletionHandler<CompletionHandler>
{
    using Base = detail::AssociatedCompletionHandler<CompletionHandler>;

    Payload payload;

    explicit GrpcCompletionHandlerWithPayload(CompletionHandler completion_handler)
        : Base(std::move(completion_handler))
    {
    }

    decltype(auto) operator()(bool ok) &&
    {
        return static_cast<Base&&>(*this)(std::pair{std::move(this->payload), ok});
    }
};

template <class Payload, class InitiatingFunction>
struct GrpcWithPayloadInitiator : detail::GrpcInitiator<InitiatingFunction>
{
    using detail::GrpcInitiator<InitiatingFunction>::GrpcInitiator;

    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler)
    {
        detail::GrpcInitiator<InitiatingFunction>::operator()(
            detail::GrpcCompletionHandlerWithPayload<CompletionHandler, Payload>{std::move(completion_handler)});
    }
};

template <class Payload, class InitiatingFunction, class CompletionToken>
auto grpc_initiate_with_payload(InitiatingFunction initiating_function, CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void(std::pair<Payload, bool>)>(
        detail::GrpcWithPayloadInitiator<Payload, InitiatingFunction>{std::move(initiating_function)}, token);
}
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INITIATE_HPP
