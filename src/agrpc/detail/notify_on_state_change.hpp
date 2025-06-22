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

#ifndef AGRPC_DETAIL_NOTIFY_ON_STATE_CHANGE_HPP
#define AGRPC_DETAIL_NOTIFY_ON_STATE_CHANGE_HPP

#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/channel.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Deadline>
struct NotifyOnStateChangeInitFunction
{
    grpc::ChannelInterface& channel_;
    Deadline deadline_;
    ::grpc_connectivity_state last_observed_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        channel_.NotifyOnStateChange(last_observed_, deadline_, grpc_context.get_completion_queue(), tag);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NOTIFY_ON_STATE_CHANGE_HPP
