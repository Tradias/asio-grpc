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

#ifndef AGRPC_DETAIL_INITIATE_SENDER_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_INITIATE_SENDER_IMPLEMENTATION_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/async_initiate.hpp>
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/conditional_sender.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/use_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
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
                static_cast<CompletionHandler&&>(completion_handler)),
            initiation, static_cast<Implementation&&>(implementation));
    }

    agrpc::GrpcContext& grpc_context;
};

struct ConditionalSubmitSenderToWorkTrackingCompletionHandler
{
    template <class CompletionHandler, class Implementation, class... Args>
    void operator()(CompletionHandler&& completion_handler,
                    const typename detail::RemoveCrefT<Implementation>::Initiation& initiation,
                    Implementation&& implementation, bool condition, Args&&... args)
    {
        using Signature = typename detail::RemoveCrefT<Implementation>::Signature;
        if (condition)
        {
            detail::SubmitSenderToWorkTrackingCompletionHandler{grpc_context}(
                static_cast<CompletionHandler&&>(completion_handler), initiation,
                static_cast<Implementation&&>(implementation));
        }
        else
        {
            detail::InitiateImmediateCompletion<Signature>{}(static_cast<CompletionHandler&&>(completion_handler),
                                                             static_cast<Args&&>(args)...);
        }
    }

    agrpc::GrpcContext& grpc_context;
};
#endif

template <class Implementation, class CompletionToken>
auto async_initiate_sender_implementation(agrpc::GrpcContext& grpc_context,
                                          const typename Implementation::Initiation& initiation,
                                          Implementation&& implementation, [[maybe_unused]] CompletionToken& token)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, CompletionToken>)
    {
        return asio::async_initiate<CompletionToken, typename Implementation::Signature>(
            detail::SubmitSenderToWorkTrackingCompletionHandler{grpc_context}, token, initiation,
            static_cast<Implementation&&>(implementation));
    }
    else
#endif
    {
        return detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation,
                                                                 static_cast<Implementation&&>(implementation));
    }
}

template <class Implementation, class CompletionToken, class... Args>
auto async_initiate_conditional_sender_implementation(agrpc::GrpcContext& grpc_context,
                                                      const typename Implementation::Initiation& initiation,
                                                      Implementation&& implementation, bool condition,
                                                      [[maybe_unused]] CompletionToken& token, Args&&... args)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, CompletionToken>)
    {
        using Signature = typename Implementation::Signature;
        return asio::async_initiate<CompletionToken, Signature>(
            detail::ConditionalSubmitSenderToWorkTrackingCompletionHandler{grpc_context}, token, initiation,
            static_cast<Implementation&&>(implementation), condition, static_cast<Args&&>(args)...);
    }
    else
#endif
    {
        return detail::ConditionalSenderAccess::create(
            detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation,
                                                              static_cast<Implementation&&>(implementation)),
            condition, static_cast<Args&&>(args)...);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INITIATE_SENDER_IMPLEMENTATION_HPP
