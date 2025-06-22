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
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/client_rpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <iostream>

namespace asio = boost::asio;

using ExampleStub = example::v1::Example::Stub;
using ExampleExtStub = example::v1::ExampleExt::Stub;

// Example showing some of the features of the ClientRPC API of asio-grpc with Boost.Asio.

// begin-snippet: client-side-client-rpc-streaming

// A simple client-streaming request with coroutines.

// end-snippet
asio::awaitable<void> make_client_streaming_request(agrpc::GrpcContext& grpc_context, ExampleStub& stub)
{
    using RPC = agrpc::ClientRPC<&ExampleStub::PrepareAsyncClientStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Response response;
    const bool start_ok = co_await rpc.start(stub, response);
    abort_if_not(start_ok);

    // Optionally read initial metadata first. Otherwise it will be read along with the first write.
    const bool read_ok = co_await rpc.read_initial_metadata();

    // Send a message.
    example::v1::Request request;
    const bool write_ok = co_await rpc.write(request);

    // Wait for the server to recieve all our messages and obtain the server's response + status.
    const grpc::Status status = co_await rpc.finish();
    abort_if_not(status.ok());

    std::cout << "ClientRPC: Client streaming completed. Response: " << response.integer() << '\n';

    silence_unused(read_ok, write_ok);
}
// ---------------------------------------------------
//

// begin-snippet: client-rpc-server-streaming

// A simple server-streaming request with coroutines.

// end-snippet
asio::awaitable<void> make_server_streaming_request(agrpc::GrpcContext& grpc_context, ExampleStub& stub)
{
    using RPC = agrpc::ClientRPC<&ExampleStub::PrepareAsyncServerStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(5);
    abort_if_not(co_await rpc.start(stub, request));

    example::v1::Response response;

    while (co_await rpc.read(response))
    {
        std::cout << "ClientRPC: Server streaming: " << response.integer() << "\n";
    }

    const grpc::Status status = co_await rpc.finish();
    abort_if_not(status.ok());

    std::cout << "ClientRPC: Server streaming completed\n";
}
// ---------------------------------------------------
//

// A server-streaming request that is cancelled.
asio::awaitable<void> make_server_streaming_notify_when_done_request(agrpc::GrpcContext& grpc_context,
                                                                     ExampleExtStub& stub)
{
    using RPC = agrpc::ClientRPC<&ExampleExtStub::PrepareAsyncServerStreamingNotifyWhenDone>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(1);
    abort_if_not(co_await rpc.start(stub, request));

    example::v1::Response response;
    co_await rpc.read(response);

    // cancellation also happens automatically at the end of this scope
    rpc.cancel();

    const grpc::Status status = co_await rpc.finish();
    abort_if_not(grpc::StatusCode::CANCELLED == status.error_code());

    std::cout << "ClientRPC: Server streaming notify_when_done completed\n";
}
// ---------------------------------------------------
//

// begin-snippet: client-rpc-bidirectional-streaming

// A bidirectional-streaming request that simply sends the response from the server back to it.

// end-snippet
asio::awaitable<void> make_bidirectional_streaming_request(agrpc::GrpcContext& grpc_context, ExampleStub& stub)
{
    using RPC = agrpc::ClientRPC<&ExampleStub::PrepareAsyncBidirectionalStreaming>;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    if (!co_await rpc.start(stub))
    {
        // Channel is either permanently broken or transiently broken but with the fail-fast option.
        co_return;
    }

    // Perform a request/response ping-pong.
    example::v1::Request request;
    request.set_integer(1);
    example::v1::Response response;

    // Reads and writes can be performed simultaneously.
    using namespace asio::experimental::awaitable_operators;
    auto [read_ok, write_ok] =
        co_await (rpc.read(response, asio::use_awaitable) && rpc.write(request, asio::use_awaitable));

    int count{};
    while (read_ok && write_ok && count < 10)
    {
        std::cout << "ClientRPC: Bidirectional streaming: " << response.integer() << '\n';
        request.set_integer(response.integer());
        ++count;
        std::tie(read_ok, write_ok) =
            co_await (rpc.read(response, asio::use_awaitable) && rpc.write(request, asio::use_awaitable));
    }

    // Finish will automatically signal that the client is done writing. Optionally call rpc.writes_done() to explicitly
    // signal it earlier.
    const grpc::Status status = co_await rpc.finish();
    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

// begin-snippet: client-side-run-with-deadline

// A unary request with a per-RPC step timeout. Using a unary RPC for demonstration purposes, the same mechanism can be
// applied to streaming RPCs, where it is arguably more useful.
// For unary RPCs, `grpc::ClientContext::set_deadline` should be preferred.

// end-snippet
asio::awaitable<void> make_and_cancel_unary_request(agrpc::GrpcContext& grpc_context, ExampleExtStub& stub)
{
    using RPC = agrpc::ClientRPC<&ExampleExtStub::PrepareAsyncSlowUnary>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    RPC::Request request;
    request.set_delay(2000);  // tell server to delay response by 2000ms
    RPC::Response response;

    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);

    const auto result =
        co_await asio::experimental::make_parallel_group(
            RPC::request(grpc_context, stub, client_context, request, response, asio::deferred),
            agrpc::Alarm(grpc_context)
                .wait(std::chrono::system_clock::now() + std::chrono::milliseconds(100), asio::deferred))
            .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);

    // Alternative, slighlty less performant syntax:
    //
    // using namespace asio::experimental::awaitable_operators;
    // co_await (RPC::request(grpc_context, stub, client_context, request, response) ||
    // agrpc::Alarm(grpc_context).wait(deadline))

    abort_if_not(grpc::StatusCode::CANCELLED == std::get<1>(result).error_code());
    abort_if_not(std::chrono::steady_clock::now() < not_to_exceed);
}
// ---------------------------------------------------
//

// The Shutdown endpoint is used by unit tests.
asio::awaitable<void> make_shutdown_request(agrpc::GrpcContext& grpc_context, ExampleExtStub& stub)
{
    using RPC = agrpc::ClientRPC<&ExampleExtStub::PrepareAsyncShutdown>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    google::protobuf::Empty response;
    const grpc::Status status = co_await RPC::request(grpc_context, stub, client_context, {}, response);

    if (status.ok())
    {
        std::cout << "ClientRPC: Successfully send shutdown request to server\n";
    }
    else
    {
        std::cout << "ClientRPC: Failed to send shutdown request to server: " << status.error_message() << '\n';
    }
    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const char* port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    ExampleStub stub{channel};
    ExampleExtStub stub_ext{channel};
    agrpc::GrpcContext grpc_context;

    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await make_client_streaming_request(grpc_context, stub);
            co_await make_server_streaming_request(grpc_context, stub);
            co_await make_server_streaming_notify_when_done_request(grpc_context, stub_ext);
            co_await make_bidirectional_streaming_request(grpc_context, stub);
            co_await make_and_cancel_unary_request(grpc_context, stub_ext);
            co_await make_shutdown_request(grpc_context, stub_ext);
        },
        example::RethrowFirstArg{});

    grpc_context.run();
}