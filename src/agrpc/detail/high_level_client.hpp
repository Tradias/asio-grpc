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
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/use_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Sender, class CompletionToken>
auto async_initiate_sender(Sender&& sender, CompletionToken& token)
{
    return asio::async_initiate<CompletionToken, typename detail::RemoveCrefT<Sender>::Signature>(
        [&](auto&& completion_handler, auto&& sender)
        {
            using CH = decltype(completion_handler);
            std::move(sender).submit(
                detail::CompletionHandlerReceiver<detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CH>>>(
                    std::forward<CH>(completion_handler)));
        },
        token, std::forward<Sender>(sender));
}
#endif

template <class Sender>
auto async_initiate_sender(Sender&& sender, agrpc::UseSender)
{
    return std::forward<Sender>(sender);
}

template <class Implementation, class CompletionToken>
auto async_initiate_sender_implementation(agrpc::GrpcContext& grpc_context,
                                          const typename Implementation::Initiation& initiation,
                                          Implementation implementation, CompletionToken& token)
{
    return detail::async_initiate_sender(
        detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation, std::move(implementation)), token);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP
