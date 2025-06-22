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

#ifndef AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_MIXIN_HPP
#define AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_MIXIN_HPP

#include <agrpc/detail/server_rpc_context_base.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief ServerRPC NotifyWhenDone base
 *
 * @since 2.7.0
 */
template <bool IsNotifyWhenDone, class Responder, class Executor>
class ServerRPCNotifyWhenDoneMixin : public RPCExecutorBase<Executor>,
                                     public ServerRPCResponderAndNotifyWhenDone<Responder, IsNotifyWhenDone>
{
  public:
    /**
     * @brief Is this rpc done?
     *
     * Only available if `Traits` contain `NOTIFY_WHEN_DONE = true`.
     *
     * Returns true if NotifyWhenDone has fired which indicates that `finish()` has been called or that the rpc is dead
     * (i.e., canceled, deadline expired, other side dropped the channel, etc).
     *
     * Thread-safe
     */
    [[nodiscard]] bool is_done() const noexcept { return !this->event_.is_running(); }

    /**
     * @brief Wait for done
     *
     * Only available if `Traits` contain `NOTIFY_WHEN_DONE = true`.
     *
     * Request notification of the completion of this rpc, either due to calling `finish()` or because the rpc is dead
     * (i.e., canceled, deadline expired, other side dropped the channel, etc).
     * [rpc.context().IsCancelled()](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#af2d0f087805b4b475d01b12d73508f09)
     * may only be called after this operation completes.
     *
     * Cancelling this operation does not invoke
     * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301).
     *
     * Internally, this operation uses
     * [grpc::ServerContext::AsyncNotifyWhenDone](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context_base.html#ad51d67a4bd5a0960b4c15783e10a72a3).
     *
     * @attention Only one call to `wait_for_done()` may be outstanding at a time.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void()`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_done(CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return this->event_.wait(RPCExecutorBaseAccess::grpc_context(*this), static_cast<CompletionToken&&>(token));
    }

  protected:
    using RPCExecutorBase<Executor>::RPCExecutorBase;
};

template <class Responder, class Executor>
class ServerRPCNotifyWhenDoneMixin<false, Responder, Executor>
    : public RPCExecutorBase<Executor>, public ServerRPCResponderAndNotifyWhenDone<Responder, false>
{
  protected:
    using RPCExecutorBase<Executor>::RPCExecutorBase;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_MIXIN_HPP
