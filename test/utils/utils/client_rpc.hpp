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

#ifndef AGRPC_UTILS_CLIENT_RPC_HPP
#define AGRPC_UTILS_CLIENT_RPC_HPP

#include "test/v1/test.grpc.pb.h"

#include <agrpc/client_rpc.hpp>
#include <doctest/doctest.h>

namespace test
{
using UnaryClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncUnary>;
using UnaryInterfaceClientRPC = agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncUnary>;
using ClientStreamingClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming>;
using ClientStreamingInterfaceClientRPC = agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncClientStreaming>;
using ServerStreamingClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming>;
using ServerStreamingInterfaceClientRPC = agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncServerStreaming>;
using BidirectionalStreamingClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming>;
using BidirectionalStreamingInterfaceClientRPC =
    agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncBidirectionalStreaming>;
using GenericUnaryClientRPC = agrpc::GenericUnaryClientRPC;
using GenericStreamingClientRPC = agrpc::GenericStreamingClientRPC;
}

TYPE_TO_STRING(test::UnaryClientRPC);
TYPE_TO_STRING(test::UnaryInterfaceClientRPC);
TYPE_TO_STRING(test::ClientStreamingClientRPC);
TYPE_TO_STRING(test::ClientStreamingInterfaceClientRPC);
TYPE_TO_STRING(test::ServerStreamingClientRPC);
TYPE_TO_STRING(test::ServerStreamingInterfaceClientRPC);
TYPE_TO_STRING(test::BidirectionalStreamingClientRPC);
TYPE_TO_STRING(test::BidirectionalStreamingInterfaceClientRPC);
TYPE_TO_STRING(test::GenericUnaryClientRPC);
TYPE_TO_STRING(test::GenericStreamingClientRPC);

#endif  // AGRPC_UTILS_CLIENT_RPC_HPP
