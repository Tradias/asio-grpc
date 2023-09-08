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

#ifndef AGRPC_DETAIL_SERVER_RPC_READ_MIXIN_HPP
#define AGRPC_DETAIL_SERVER_RPC_READ_MIXIN_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/running_manual_reset_event.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct ServerRPCReadMixinAccess;

template <bool IsResumableRead, class Base>
class ServerRPCReadMixin : public Base
{
  public:
    template <class Request>
    void initiate_read(Request& request)
    {
        RPCExecutorBaseAccess::grpc_context(*this).work_started();
        ServerRPCContextBaseAccess::responder(*this).Read(&request, event_.tag());
    }

    template <class CompletionToken = RPCExecutorBaseAccess::DefaultCompletionTokenT<Base>>
    auto wait_for_read(CompletionToken&& token = RPCExecutorBaseAccess::DefaultCompletionTokenT<Base>{})
    {
        return event_.wait(RPCExecutorBaseAccess::grpc_context(*this), static_cast<CompletionToken&&>(token));
    }

  protected:
    using Base::Base;

  private:
    friend detail::ServerRPCReadMixinAccess;

    RunningManualResetEvent<void(bool)> event_;
};

template <class Base>
class ServerRPCReadMixin<false, Base> : public Base
{
  protected:
    using Base::Base;
};

struct ServerRPCReadMixinAccess
{
    template <bool IsResumableRead, class Base>
    [[nodiscard]] static bool is_reading(ServerRPCReadMixin<IsResumableRead, Base>& mixin) noexcept
    {
        return mixin.event_.is_running();
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_READ_MIXIN_HPP
