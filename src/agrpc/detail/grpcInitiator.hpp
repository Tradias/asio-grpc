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

#ifndef AGRPC_DETAIL_GRPCINITIATOR_HPP
#define AGRPC_DETAIL_GRPCINITIATOR_HPP

#include <agrpc/detail/allocateOperation.hpp>
#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/associatedCompletionHandler.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpcContextImplementation.hpp>
#include <agrpc/detail/grpcSender.hpp>
#include <agrpc/detail/grpcSubmit.hpp>
#include <agrpc/detail/queryGrpcContext.hpp>
#include <agrpc/detail/unbind.hpp>
#include <agrpc/detail/useSender.hpp>
#include <agrpc/grpcContext.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class InitiatingFunction, class StopFunction = detail::Empty>
class GrpcInitiator
{
  public:
    explicit GrpcInitiator(InitiatingFunction initiating_function) : initiating_function(std::move(initiating_function))
    {
    }

    template <class CompletionHandler>
    void operator()(CompletionHandler&& completion_handler)
    {
        auto unbound = detail::unbind_and_get_associates(std::forward<CompletionHandler>(completion_handler));
        this->submit(unbound, std::move(unbound.completion_handler()));
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
        detail::grpc_submit(grpc_context, std::move(this->initiating_function),
                            std::forward<CompletionHandler>(completion_handler), unbound.allocator());
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
    explicit GrpcCompletionHandlerWithPayload(Args&&... args) : Base(std::forward<Args>(args)...)
    {
    }

    decltype(auto) operator()(bool ok) &&
    {
        return static_cast<Base&&>(*this)(std::pair{std::move(this->payload_), ok});
    }

    [[nodiscard]] constexpr auto& payload() noexcept { return payload_; }

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
        auto unbound = detail::unbind_and_get_associates(std::forward<CompletionHandler>(completion_handler));
        using UnboundCompletionHandler = typename decltype(unbound)::CompletionHandlerT;
        detail::GrpcCompletionHandlerWithPayload<UnboundCompletionHandler, Payload>
            unbound_completion_handler_with_payload{std::move(unbound.completion_handler())};
        this->submit(unbound, std::move(unbound_completion_handler_with_payload));
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCINITIATOR_HPP
