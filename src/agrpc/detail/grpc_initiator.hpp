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

#ifndef AGRPC_DETAIL_GRPC_INITIATOR_HPP
#define AGRPC_DETAIL_GRPC_INITIATOR_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/associated_completion_handler.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/grpc_submit.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/unbind.hpp>
#include <agrpc/detail/use_sender.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class InitiatingFunction, class StopFunction = detail::Empty>
class GrpcInitiator
{
  public:
    explicit GrpcInitiator(InitiatingFunction initiating_function)
        : initiating_function(static_cast<InitiatingFunction&&>(initiating_function))
    {
    }

    template <class CompletionHandler>
    void operator()(CompletionHandler&& completion_handler)
    {
        auto unbound = detail::unbind_and_get_associates(static_cast<CompletionHandler&&>(completion_handler));
        submit(unbound, std::move(unbound.completion_handler()));
    }

  protected:
    template <class Unbound, class CompletionHandler>
    void submit(Unbound& unbound, CompletionHandler&& completion_handler)
    {
        auto& grpc_context = detail::query_grpc_context(unbound.executor());
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
        {
            return;
        }
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if constexpr (!std::is_same_v<detail::Empty, StopFunction>)
        {
            if (unbound.cancellation_slot().is_connected())
            {
                unbound.cancellation_slot().template emplace<StopFunction>(initiating_function);
            }
        }
#endif
        detail::grpc_submit(grpc_context, initiating_function, static_cast<CompletionHandler&&>(completion_handler));
    }

  private:
    InitiatingFunction initiating_function;
};

template <class CompletionHandler, class Payload>
class GrpcCompletionHandlerWithPayload : public detail::AssociatedCompletionHandler<CompletionHandler>
{
  private:
    using Base = detail::AssociatedCompletionHandler<CompletionHandler>;

  public:
    template <class... Args>
    explicit GrpcCompletionHandlerWithPayload(Args&&... args) : Base(static_cast<Args&&>(args)...)
    {
    }

    decltype(auto) operator()(bool ok) &&
    {
        return static_cast<Base&&>(*this)(std::pair{static_cast<Payload&&>(payload_), ok});
    }

    [[nodiscard]] auto& payload() noexcept { return payload_; }

  private:
    Payload payload_;
};

template <class Payload, class InitiatingFunction>
class GrpcWithPayloadInitiator : public detail::GrpcInitiator<InitiatingFunction>
{
  public:
    using detail::GrpcInitiator<InitiatingFunction>::GrpcInitiator;

    template <class CompletionHandler>
    void operator()(CompletionHandler&& completion_handler)
    {
        auto unbound = detail::unbind_and_get_associates(static_cast<CompletionHandler&&>(completion_handler));
        using UnboundCompletionHandler = typename decltype(unbound)::CompletionHandlerT;
        detail::GrpcCompletionHandlerWithPayload<UnboundCompletionHandler, Payload>
            unbound_completion_handler_with_payload{std::move(unbound.completion_handler())};
        this->submit(unbound, std::move(unbound_completion_handler_with_payload));
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_INITIATOR_HPP
