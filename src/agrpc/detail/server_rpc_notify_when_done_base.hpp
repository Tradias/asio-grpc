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

#ifndef AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_BASE_HPP
#define AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_BASE_HPP

#include <agrpc/detail/async_initiate.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/server_rpc_notify_when_done.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <bool IsNotifyWhenDone>
class ServerRPCNotifyWhenDoneBase
{
  public:
    bool is_running() const { return notify_when_done_.is_running(); }

    template <class CompletionToken>
    auto done(CompletionToken&& token)
    {
        return notify_when_done_.done(static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ServerRPCContextBaseAccess;

    void initiate(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context)
    {
        notify_when_done_.initiate(grpc_context, server_context);
    }

    detail::NotifyWhenDone notify_when_done_{};
};

template <>
class ServerRPCNotifyWhenDoneBase<false>
{
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_BASE_HPP
