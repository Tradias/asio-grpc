// Copyright 2022 Dennis Hezel
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
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/high_level_client.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <iostream>

namespace asio = boost::asio;

// Example showing some of the features of the high-level client API of asio-grpc with Boost.Asio.

// begin-snippet: client-side-high-level-client-streaming
// ---------------------------------------------------
// A simple client-streaming request with coroutines.
// ---------------------------------------------------
// end-snippet
asio::awaitable<void> make_client_streaming_request(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncClientStreaming>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Response response;
    RPC rpc = co_await RPC::request(grpc_context, stub, client_context, response);
    abort_if_not(rpc.ok());

    // Optionally read initial metadata first. Otherwise it will be read along with the first write.
    bool read_ok = co_await rpc.read_initial_metadata();

    // Send a message. On error, rpc.status() will be populated with error information.
    example::v1::Request request;
    bool write_ok = co_await rpc.write(request);

    // Wait for the server to recieve all our messages and obtain the server's response + status.
    bool status_ok = co_await rpc.finish();

    // In case of an error inspect the status for details.
    grpc::Status& status = rpc.status();

    abort_if_not(status_ok);
    silence_unused(read_ok, write_ok, status);

    std::cout << "High-level: Client streaming completed\n";
}
// ---------------------------------------------------
//

// begin-snippet: client-side-high-level-server-streaming
// ---------------------------------------------------
// A simple server-streaming request with coroutines.
// ---------------------------------------------------
// end-snippet
asio::awaitable<void> make_server_streaming_request(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncServerStreaming>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(5);
    RPC rpc = co_await RPC::request(grpc_context, stub, client_context, request);
    abort_if_not(rpc.ok());

    example::v1::Response response;

    while (co_await rpc.read(response))
    {
        std::cout << "High-level: Server streaming: " << response.integer() << "\n";
    }

    if (!rpc.ok())
    {
        // In case of an error inspect the status for details.
        grpc::Status& status = rpc.status();
        abort_if_not(status.ok());
    }

    std::cout << "High-level: Server streaming completed\n";
}
// ---------------------------------------------------
//

// begin-snippet: client-side-high-level-bidirectional-streaming
// ---------------------------------------------------
// A bidirectional-streaming request that simply sends the response from the server back to it.
// ---------------------------------------------------
// end-snippet
asio::awaitable<void> make_bidirectional_streaming_request(agrpc::GrpcContext& grpc_context,
                                                           example::v1::Example::Stub& stub)
{
    using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    RPC rpc = co_await RPC::request(grpc_context, stub, client_context);
    if (!rpc.ok())
    {
        // Channel is either permanently broken or transiently broken but with the fail-fast option.
        co_return;
    }

    // Perform a request/response ping-pong.
    example::v1::Request request;
    request.set_integer(1);
    bool write_ok{true};
    bool read_ok{true};
    int count{};
    while (read_ok && write_ok && count < 10)
    {
        example::v1::Response response;
        // Reads and writes can be performed simultaneously.
        using namespace asio::experimental::awaitable_operators;
        std::tie(read_ok, write_ok) = co_await (rpc.read(response) && rpc.write(request));

        std::cout << "High-level: Bidirectional streaming: " << response.integer() << '\n';
        request.set_integer(response.integer());
        ++count;
    }

    // Finish will automatically signal that the client is done writing. Optionally call rpc.writes_done() to explicitly
    // signal it earlier.
    bool status_ok = co_await rpc.finish();

    abort_if_not(status_ok);
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The Shutdown endpoint is used by unit tests.
// ---------------------------------------------------
asio::awaitable<void> make_shutdown_request(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    google::protobuf::Empty response;
    const grpc::Status status = co_await agrpc::RPC<&example::v1::ExampleExt::Stub::PrepareAsyncShutdown>::request(
        grpc_context, stub, client_context, {}, response);

    if (status.ok())
    {
        std::cout << "High-level: Successfully send shutdown request to server\n";
    }
    else
    {
        std::cout << "High-level: Failed to send shutdown request to server: " << status.error_message() << '\n';
    }
    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    example::v1::Example::Stub stub{channel};
    example::v1::ExampleExt::Stub stub_ext{channel};
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await make_client_streaming_request(grpc_context, stub);
            co_await make_server_streaming_request(grpc_context, stub);
            co_await make_bidirectional_streaming_request(grpc_context, stub);
            co_await make_shutdown_request(grpc_context, stub_ext);
        },
        asio::detached);

    grpc_context.run();
}