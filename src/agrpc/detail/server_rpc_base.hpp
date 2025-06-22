// Copyright 2025 Dennis Hezel
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

#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/server_rpc_notify_when_done_mixin.hpp>
#include <agrpc/detail/server_rpc_sender.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief ServerRPC base
 *
 * @since 2.7.0
 */
template <class Responder, class Traits, class Executor>
class ServerRPCBase : public ServerRPCNotifyWhenDoneMixin<Traits::NOTIFY_WHEN_DONE, Responder, Executor>
{
  public:
    /**
     * @brief Send initial metadata
     *
     * Request notification of the sending of initial metadata to the client.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the
     * `finish()`/`finish_with_error()` method.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
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
