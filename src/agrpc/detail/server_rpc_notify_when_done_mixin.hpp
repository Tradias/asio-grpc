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

#ifndef AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_MIXIN_HPP
#define AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_MIXIN_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <bool IsNotifyWhenDone, class Responder, class Executor>
class ServerRPCNotifyWhenDoneMixin : public RPCExecutorBase<Executor>,
                                     public ServerRPCResponderAndNotifyWhenDone<Responder, IsNotifyWhenDone>
{
  public:
    [[nodiscard]] bool is_done() const noexcept { return !this->event_.is_running(); }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_done(CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return this->event_.wait(RPCExecutorBaseAccess::grpc_context(*this), static_cast<CompletionToken&&>(token));
    }

  private:
    using RPCExecutorBase<Executor>::RPCExecutorBase;
};

template <class Responder, class Executor>
class ServerRPCNotifyWhenDoneMixin<false, Responder, Executor>
    : public RPCExecutorBase<Executor>, public ServerRPCResponderAndNotifyWhenDone<Responder, false>
{
  private:
    using RPCExecutorBase<Executor>::RPCExecutorBase;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_MIXIN_HPP
