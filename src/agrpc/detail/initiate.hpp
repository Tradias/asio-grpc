// Copyright 2021 Dennis Hezel
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
#include "agrpc/detail/completionHandlerWithPayload.hpp"
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

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Function>
struct GrpcInitiator
{
    using executor_type = asio::associated_executor_t<Function>;
    using allocator_type = asio::associated_allocator_t<Function>;

    Function function;

    explicit GrpcInitiator(Function function) : function(std::move(function)) {}

    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler)
    {
        const auto [executor, allocator] = detail::get_associated_executor_and_allocator(completion_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        if AGRPC_UNLIKELY (grpc_context.is_stopped())
        {
            return;
        }
        detail::grpc_submit(grpc_context, std::move(this->function), std::move(completion_handler), allocator);
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return asio::get_associated_executor(this->function); }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(this->function);
    }
};

template <class Payload, class Function>
struct GrpcWithPayloadInitiator : detail::GrpcInitiator<Function>
{
    using detail::GrpcInitiator<Function>::GrpcInitiator;

    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler)
    {
        detail::GrpcInitiator<Function>::operator()(
            detail::make_completion_handler_with_payload<Payload>(std::move(completion_handler)));
    }
};

template <class Payload, class Function, class CompletionToken>
auto grpc_initiate_with_payload(Function function, CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void(std::pair<Payload, bool>)>(
        detail::GrpcWithPayloadInitiator<Payload, Function>{std::move(function)}, token);
}
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INITIATE_HPP
