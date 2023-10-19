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

#ifndef AGRPC_UTILS_INTROSPECT_RPC_HPP
#define AGRPC_UTILS_INTROSPECT_RPC_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"

#include <agrpc/client_rpc.hpp>
#include <agrpc/server_rpc.hpp>

namespace test
{
template <class RPC, auto Type = RPC::TYPE>
struct IntrospectRPC;

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::UNARY>
{
    using ClientRPC = agrpc::ClientRPC<PrepareAsync, Executor>;
    using ServerRPC =
        agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestUnary, agrpc::DefaultServerRPCTraits, Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename ClientRPC::Stub& stub, grpc::ClientContext& context,
                        const typename ClientRPC::Request& request, typename ClientRPC::Response& response,
                        CompletionToken&& token)
    {
        return ClientRPC::request(executor, stub, context, request, response, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::GenericUnaryClientRPC<Executor>, agrpc::ClientRPCType::GENERIC_UNARY>
{
    using ClientRPC = agrpc::GenericUnaryClientRPC<Executor>;
    using ServerRPC = agrpc::GenericServerRPC<agrpc::DefaultServerRPCTraits, Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename ClientRPC::Stub& stub, grpc::ClientContext& context,
                        const typename ClientRPC::Request& request, typename ClientRPC::Response& response,
                        CompletionToken&& token)
    {
        return ClientRPC::request(executor, "/test.v1.Test/Unary", stub, context, request, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::CLIENT_STREAMING>
{
    using ClientRPC = agrpc::ClientRPC<PrepareAsync, Executor>;
    using ServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestClientStreaming,
                                       agrpc::DefaultServerRPCTraits, Executor>;

    template <class CompletionToken>
    static auto start(ClientRPC& rpc, typename ClientRPC::Stub& stub, const typename ClientRPC::Request&,
                      typename ClientRPC::Response& response, CompletionToken&& token)
    {
        return rpc.start(stub, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::SERVER_STREAMING>
{
    using ClientRPC = agrpc::ClientRPC<PrepareAsync, Executor>;
    using ServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestServerStreaming,
                                       agrpc::DefaultServerRPCTraits, Executor>;

    template <class CompletionToken>
    static auto start(ClientRPC& rpc, typename ClientRPC::Stub& stub, const typename ClientRPC::Request& request,
                      const typename ClientRPC::Response&, CompletionToken&& token)
    {
        return rpc.start(stub, request, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::BIDIRECTIONAL_STREAMING>
{
    using ClientRPC = agrpc::ClientRPC<PrepareAsync, Executor>;
    using ServerRPC = agrpc::ServerRPC<&test::v1::Test::AsyncService::RequestBidirectionalStreaming,
                                       agrpc::DefaultServerRPCTraits, Executor>;

    template <class CompletionToken>
    static auto start(ClientRPC& rpc, typename ClientRPC::Stub& stub, const typename ClientRPC::Request&,
                      const typename ClientRPC::Response&, CompletionToken&& token)
    {
        return rpc.start(stub, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::GenericStreamingClientRPC<Executor>, agrpc::ClientRPCType::GENERIC_STREAMING>
{
    using ClientRPC = agrpc::GenericStreamingClientRPC<Executor>;
    using ServerRPC = agrpc::GenericServerRPC<agrpc::DefaultServerRPCTraits, Executor>;

    template <class CompletionToken>
    static auto start(ClientRPC& rpc, typename ClientRPC::Stub& stub, const typename ClientRPC::Request&,
                      const typename ClientRPC::Response&, CompletionToken&& token)
    {
        return rpc.start("/test.v1.Test/BidirectionalStreaming", stub, token);
    }
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::UNARY>
{
    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncUnary, Executor>;
    using ServerRPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::CLIENT_STREAMING>
{
    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming, Executor>;
    using ServerRPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::SERVER_STREAMING>
{
    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming, Executor>;
    using ServerRPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::BIDIRECTIONAL_STREAMING>
{
    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming, Executor>;
    using ServerRPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <class Traits, class Executor>
struct IntrospectRPC<agrpc::GenericServerRPC<Traits, Executor>, agrpc::ServerRPCType::GENERIC>
{
    using ClientRPC = agrpc::ClientRPC<agrpc::ClientRPCType::GENERIC_STREAMING, Executor>;
    using ServerRPC = agrpc::GenericServerRPC<Traits, Executor>;
};
}

#endif  // AGRPC_UTILS_INTROSPECT_RPC_HPP
