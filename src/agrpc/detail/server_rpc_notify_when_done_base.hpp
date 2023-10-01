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
#include <agrpc/detail/notify_when_done_event.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <bool>
class ServerRPCNotifyWhenDoneBase
{
  private:
    friend detail::ServerRPCContextBaseAccess;

    template <bool, class, class>
    friend class detail::ServerRPCNotifyWhenDoneMixin;

    void initiate(grpc::ServerContext& server_context) { server_context.AsyncNotifyWhenDone(event_.tag()); }

    NotifyWhenDoneEvent event_;
};

template <>
class ServerRPCNotifyWhenDoneBase<false>
{
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_BASE_HPP
