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

#include "example/v1/example.grpc.pb.h"

#include <agrpc/client_rpc.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

/* [client-rpc-unary-initial-metadata] */
asio::awaitable<void> client_rpc_unary_initial_metadata(agrpc::GrpcContext& grpc_context,
                                                        example::v1::Example::Stub& stub)
{
    using RPC = agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncUnary>;
    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    RPC::Request request;
    rpc.start(stub, request);
    co_await rpc.read_initial_metadata();
    // Do something with:
    // rpc.context().GetServerInitialMetadata();
    RPC::Response response;
    const grpc::Status status = co_await rpc.finish(response);
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
}
/* [client-rpc-unary-initial-metadata] */

/* [client-rpc-generic-unary] */
asio::awaitable<void> client_rpc_generic_unary(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    grpc::ByteBuffer request_buffer;
    bool own_buffer;
    grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Request>(request, &request_buffer, &own_buffer);
    (void)own_buffer;

    grpc::ByteBuffer response_buffer;

    using RPC = agrpc::GenericUnaryClientRPC;
    if (const grpc::Status status =
            co_await RPC::request(grpc_context, "/example.v1.Example/Unary", stub, client_context, request_buffer,
                                  response_buffer, asio::use_awaitable);
        !status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    example::v1::Response response;
    if (const grpc::Status status =
            grpc::GenericDeserialize<grpc::ProtoBufferReader, example::v1::Response>(&response_buffer, &response);
        !status.ok())
    {
        std::cerr << "Rpc failed: unexpected response type";
        co_return;
    }

    std::cout << "Response: " << response.integer();
}
/* [client-rpc-generic-unary] */

// Explicitly formatted using `ColumnLimit: 90`
// clang-format off
/* [client-rpc-unary] */
asio::awaitable<void> client_rpc_unary(agrpc::GrpcContext& grpc_context,
                                       example::v1::Example::Stub& stub)
{
    using RPC = agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncUnary>;
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() +
                                std::chrono::seconds(5));
    RPC::Request request;
    RPC::Response response;
    const grpc::Status status =
        co_await RPC::request(grpc_context, stub, client_context, request, response);
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
    std::cout << "Response: " << response.integer();
}
/* [client-rpc-unary] */

/* [client-rpc-client-streaming] */
asio::awaitable<void> client_rpc_client_streaming(agrpc::GrpcContext& grpc_context,
                                                  example::v1::Example::Stub& stub)
{
    using RPC =
        agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncClientStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() +
                               std::chrono::seconds(5));

    RPC::Response response;
    if (!co_await rpc.start(stub, response))
    {
        const grpc::Status status = co_await rpc.finish();
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    RPC::Request request;
    request.set_integer(1);
    while (co_await rpc.write(request) && request.integer() < 42)
    {
        request.set_integer(request.integer() + 1);
    }

    const grpc::Status status = co_await rpc.finish();
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    std::cout << "Response: " << response.integer();
}
/* [client-rpc-client-streaming] */

/* [client-rpc-server-streaming] */
asio::awaitable<void> client_rpc_server_streaming(agrpc::GrpcContext& grpc_context,
                                                  example::v1::Example::Stub& stub)
{
    using RPC =
        agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncServerStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() +
                               std::chrono::seconds(5));

    RPC::Request request;
    request.set_integer(42);
    if (!co_await rpc.start(stub, request))
    {
        const grpc::Status status = co_await rpc.finish();
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    RPC::Response response;
    while (co_await rpc.read(response))
    {
        std::cout << "Response: " << response.integer() << '\n';
    }

    const grpc::Status status = co_await rpc.finish();
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
}
/* [client-rpc-server-streaming] */

/* [client-rpc-bidirectional-streaming] */
asio::awaitable<void> client_rpc_bidirectional_streaming(agrpc::GrpcContext& grpc_context,
                                                         example::v1::Example::Stub& stub)
{
    using RPC =
        agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() +
                               std::chrono::seconds(5));

    if (!co_await rpc.start(stub))
    {
        const grpc::Status status = co_await rpc.finish();
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    RPC::Request request;
    request.set_integer(42);

    bool write_ok{true};
    RPC::Response response;
    while (co_await rpc.read(response) && write_ok)
    {
        request.set_integer(response.integer() + 1);
        write_ok = co_await rpc.write(request);
    }

    const grpc::Status status = co_await rpc.finish();
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
}
/* [client-rpc-bidirectional-streaming] */
// clang-format on
