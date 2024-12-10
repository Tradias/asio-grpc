// Copyright 2024 Dennis Hezel
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

#include "awaitable_client_rpc.hpp"
#include "example/v1/example.grpc.pb.h"
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"

#include <agrpc/asio_grpc.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

namespace asio = boost::asio;

using ExampleStub = example::v1::Example::Stub;
using ExampleExtStub = example::v1::ExampleExt::Stub;

asio::awaitable<void> make_server_streaming_request(agrpc::GrpcContext& grpc_context, ExampleStub& stub)
{
    using RPC = example::AwaitableClientRPC<&ExampleStub::PrepareAsyncServerStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(5);
    abort_if_not(co_await rpc.start(stub, request));

    example::v1::Response response;

    while (co_await rpc.read(response))
    {
        std::cout << "ClientRPC async-generator: Server streaming: " << response.integer() << "\n";
    }

    const grpc::Status status = co_await rpc.finish();
    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The Shutdown endpoint is used by unit tests.
// ---------------------------------------------------
asio::awaitable<void> make_shutdown_request(agrpc::GrpcContext& grpc_context, ExampleExtStub& stub)
{
    using RPC = example::AwaitableClientRPC<&ExampleExtStub::PrepareAsyncShutdown>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    google::protobuf::Empty response;
    const grpc::Status status = co_await RPC::request(grpc_context, stub, client_context, {}, response);

    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    ExampleStub stub{channel};
    ExampleExtStub stub_ext{channel};
    agrpc::GrpcContext grpc_context;

    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await make_server_streaming_request(grpc_context, stub);
            co_await make_shutdown_request(grpc_context, stub_ext);
        },
        example::RethrowFirstArg{});

    grpc_context.run();
}