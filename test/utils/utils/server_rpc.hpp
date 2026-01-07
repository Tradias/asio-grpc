// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_UTILS_SERVER_RPC_HPP
#define AGRPC_UTILS_SERVER_RPC_HPP

#include "test/v1/test.grpc.pb.h"

#include <agrpc/default_server_rpc_traits.hpp>
#include <agrpc/server_rpc.hpp>
#include <doctest/doctest.h>

namespace test
{
struct NotifyWhenDoneTraits : agrpc::DefaultServerRPCTraits
{
    static constexpr bool NOTIFY_WHEN_DONE = true;
};

using UnaryServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestUnary>;
using NotifyWhenDoneUnaryServerRPC =
    agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestUnary, NotifyWhenDoneTraits>;

using ClientStreamingServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestClientStreaming>;
using NotifyWhenDoneClientStreamingServerRPC =
    agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestClientStreaming, NotifyWhenDoneTraits>;

using ServerStreamingServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestServerStreaming>;
using NotifyWhenDoneServerStreamingServerRPC =
    agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestServerStreaming, NotifyWhenDoneTraits>;

using BidirectionalStreamingServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestBidirectionalStreaming>;
using NotifyWhenDoneBidirectionalStreamingServerRPC =
    agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestBidirectionalStreaming, NotifyWhenDoneTraits>;

using GenericServerRPC = agrpc::GenericServerRPC;
using NotifyWhenDoneGenericServerRPC = agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC, NotifyWhenDoneTraits>;
}

TYPE_TO_STRING(test::UnaryServerRPC);
TYPE_TO_STRING(test::NotifyWhenDoneUnaryServerRPC);

TYPE_TO_STRING(test::ClientStreamingServerRPC);
TYPE_TO_STRING(test::NotifyWhenDoneClientStreamingServerRPC);

TYPE_TO_STRING(test::ServerStreamingServerRPC);
TYPE_TO_STRING(test::NotifyWhenDoneServerStreamingServerRPC);

TYPE_TO_STRING(test::BidirectionalStreamingServerRPC);
TYPE_TO_STRING(test::NotifyWhenDoneBidirectionalStreamingServerRPC);

TYPE_TO_STRING(test::GenericServerRPC);
TYPE_TO_STRING(test::NotifyWhenDoneGenericServerRPC);

#endif  // AGRPC_UTILS_SERVER_RPC_HPP
