// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_SERVER_RPC_BASE_HPP
#define AGRPC_DETAIL_SERVER_RPC_BASE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/server_rpc_notify_when_done_mixin.hpp>
#include <agrpc/detail/server_rpc_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Responder, class Traits, class Executor>
class ServerRPCBase : public ServerRPCNotifyWhenDoneMixin<Traits::NOTIFY_WHEN_DONE, Responder, Executor>
{
  public:
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto send_initial_metadata(CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            detail::RPCExecutorBaseAccess::grpc_context(*this),
            detail::SendInitialMetadataSenderInitiation<Responder>{*this},
            detail::SendInitialMetadataSenderImplementation{}, static_cast<CompletionToken&&>(token));
    }

  protected:
    using ServerRPCNotifyWhenDoneMixin<Traits::NOTIFY_WHEN_DONE, Responder, Executor>::ServerRPCNotifyWhenDoneMixin;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_BASE_HPP
