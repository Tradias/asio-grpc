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

#ifndef AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP
#define AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/async_initiate.hpp>
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/conditional_sender.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/use_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct SubmitSenderToWorkTrackingCompletionHandler
{
    template <class CompletionHandler, class Implementation>
    void operator()(CompletionHandler&& completion_handler,
                    const typename detail::RemoveCrefT<Implementation>::Initiation& initiation,
                    Implementation&& implementation)
    {
        detail::submit_basic_sender_running_operation(
            grpc_context,
            detail::CompletionHandlerReceiver<
                detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>>(
                std::forward<CompletionHandler>(completion_handler)),
            initiation, std::forward<Implementation>(implementation));
    }

    agrpc::GrpcContext& grpc_context;
};

template <class Implementation, class CompletionToken>
auto async_initiate_sender_implementation(agrpc::GrpcContext& grpc_context,
                                          const typename Implementation::Initiation& initiation,
                                          Implementation implementation, CompletionToken& token)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, CompletionToken>)
    {
        return asio::async_initiate<CompletionToken, typename Implementation::Signature>(
            detail::SubmitSenderToWorkTrackingCompletionHandler{grpc_context}, token, initiation,
            std::move(implementation));
    }
    else
#endif
    {
        return detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation, std::move(implementation));
    }
}

template <class Implementation, class CompletionToken, class... Args>
auto async_initiate_conditional_sender_implementation(agrpc::GrpcContext& grpc_context,
                                                      const typename Implementation::Initiation& initiation,
                                                      Implementation implementation, bool condition,
                                                      CompletionToken& token, Args&&... args)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, CompletionToken>)
    {
        using Signature = typename Implementation::Signature;
        if (condition)
        {
            return asio::async_initiate<CompletionToken, Signature>(
                detail::SubmitSenderToWorkTrackingCompletionHandler{grpc_context}, token, initiation,
                std::move(implementation));
        }
        else
        {
            return detail::async_initiate_immediate_completion<Signature>(std::move(token),
                                                                          std::forward<Args>(args)...);
        }
    }
    else
#endif
    {
        return detail::ConditionalSenderAccess::create(
            detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation, std::move(implementation)),
            condition, std::forward<Args>(args)...);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP
