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
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::ClientContext& context,
                        const typename RPC::Request& request, typename RPC::Response& response, CompletionToken&& token)
    {
        return RPC::request(executor, stub, context, request, response, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::ClientRPCGenericUnary<Executor>, agrpc::ClientRPCType::GENERIC_UNARY>
{
    static constexpr auto SERVER_REQUEST = agrpc::ServerRPCType::GENERIC;

    using RPC = agrpc::ClientRPCGenericUnary<Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::ClientContext& context,
                        const typename RPC::Request& request, typename RPC::Response& response, CompletionToken&& token)
    {
        return RPC::request(executor, "/test.v1.Test/Unary", stub, context, request, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::CLIENT_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestClientStreaming;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, const typename RPC::Request&,
                      typename RPC::Response& response, CompletionToken&& token)
    {
        return rpc.start(stub, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::SERVER_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestServerStreaming;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, const typename RPC::Request& request,
                      const typename RPC::Response&, CompletionToken&& token)
    {
        return rpc.start(stub, request, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::BIDIRECTIONAL_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestBidirectionalStreaming;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, const typename RPC::Request&, const typename RPC::Response&,
                      CompletionToken&& token)
    {
        return rpc.start(stub, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::ClientRPCGenericStreaming<Executor>, agrpc::ClientRPCType::GENERIC_STREAMING>
{
    static constexpr auto SERVER_REQUEST = agrpc::ServerRPCType::GENERIC;

    using RPC = agrpc::ClientRPCGenericStreaming<Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, const typename RPC::Request&, const typename RPC::Response&,
                      CompletionToken&& token)
    {
        return rpc.start("/test.v1.Test/BidirectionalStreaming", stub, token);
    }
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::UNARY>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncUnary, Executor>;
    using RPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::CLIENT_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestClientStreaming;

    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming, Executor>;
    using RPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::SERVER_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestServerStreaming;

    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming, Executor>;
    using RPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <auto RequestRPC, class Traits, class Executor>
struct IntrospectRPC<agrpc::ServerRPC<RequestRPC, Traits, Executor>, agrpc::ServerRPCType::BIDIRECTIONAL_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestBidirectionalStreaming;

    using ClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming, Executor>;
    using RPC = agrpc::ServerRPC<RequestRPC, Traits, Executor>;
};

template <class Traits, class Executor>
struct IntrospectRPC<agrpc::GenericServerRPC<Traits, Executor>, agrpc::ServerRPCType::GENERIC>
{
    static constexpr auto SERVER_REQUEST = agrpc::ServerRPCType::GENERIC;

    using ClientRPC = agrpc::ClientRPC<agrpc::ClientRPCType::GENERIC_STREAMING, Executor>;
    using RPC = agrpc::GenericServerRPC<Traits, Executor>;
};
}

#endif  // AGRPC_UTILS_INTROSPECT_RPC_HPP
