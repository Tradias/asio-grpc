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

#include "example/v1/example.grpc.pb.h"
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "notify_when_done_server_rpc.hpp"
#include "rethrow_first_arg.hpp"
#include "server_shutdown_asio.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/thread_pool.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>

namespace asio = boost::asio;

using ExampleService = example::v1::Example::AsyncService;
using ExampleExtService = example::v1::ExampleExt::AsyncService;

// Example showing some of the features of using asio-grpc with Boost.Asio.

// begin-snippet: server-side-client-streaming

// A simple client-streaming rpc handler using C++20 coroutines.

// end-snippet
using ClientStreamingRPC = agrpc::ServerRPC<&ExampleService::RequestClientStreaming>;

asio::awaitable<void> handle_client_streaming_request(ClientStreamingRPC& rpc)
{
    // Optionally send initial metadata first.
    if (!co_await rpc.send_initial_metadata())
    {
        // Connection lost
        co_return;
    }

    bool read_ok;
    do
    {
        example::v1::Request request;
        // Read from the client stream until the client has signaled `writes_done`.
        read_ok = co_await rpc.read(request);
    } while (read_ok);

    example::v1::Response response;
    response.set_integer(42);
    co_await rpc.finish(response, grpc::Status::OK);

    // Or finish with an error
    // co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);
}
// ---------------------------------------------------
//

// begin-snippet: server-side-server-streaming

// A simple server-streaming rpc handler using C++20 coroutines.

// end-snippet
using ServerStreamingRPC = agrpc::ServerRPC<&ExampleService::RequestServerStreaming>;

asio::awaitable<void> handle_server_streaming_request(ServerStreamingRPC& rpc, example::v1::Request& request)
{
    example::v1::Response response;
    response.set_integer(request.integer());
    while (co_await rpc.write(response) && response.integer() > 0)
    {
        response.set_integer(response.integer() - 1);
    }
    co_await rpc.finish(grpc::Status::OK);
}
// ---------------------------------------------------
//

// begin-snippet: server-side-notify-when-done

// A server-streaming rpc handler that sends a message every 30s but completes immediately if the client cancels the
// rpc.

// end-snippet
using ServerStreamingNotifyWhenDoneRPC =
    example::NotifyWhenDoneServerRPC<&ExampleExtService::RequestServerStreamingNotifyWhenDone>;

auto server_streaming_notify_when_done_request_handler(agrpc::GrpcContext& grpc_context)
{
    return [&grpc_context](ServerStreamingNotifyWhenDoneRPC& rpc,
                           ServerStreamingNotifyWhenDoneRPC::Request& request) -> asio::awaitable<void>
    {
        ServerStreamingNotifyWhenDoneRPC::Response response;
        response.set_integer(request.integer());
        if (!co_await rpc.write(response))
        {
            co_return;
        }
        agrpc::Alarm alarm(grpc_context);
        while (true)
        {
            const auto [completion_order, wait_ok, ec] =
                co_await asio::experimental::make_parallel_group(
                    alarm.wait(std::chrono::system_clock::now() + std::chrono::seconds(30), asio::deferred),
                    rpc.wait_for_done(asio::deferred))
                    .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
            if (completion_order[0] == 0)
            {
                // alarm completed, send the next message to the client:
                response.set_integer(response.integer() + 1);
                if (!co_await rpc.write(response))
                {
                    co_return;
                }
            }
            else
            {
                // wait_for_done completed, IsCancelled can now be called:
                abort_if_not(rpc.context().IsCancelled());
                std::cout << "ServerRPC: Server streaming notify_when_done was successfully cancelled\n";
                co_return;
            }
        }
    };
}
// ---------------------------------------------------
//

// begin-snippet: server-side-bidirectional-streaming

// The following bidirectional-streaming example shows how to dispatch requests to a thread_pool and write responses
// back to the client.

// end-snippet
using BidiStreamingRPC = agrpc::ServerRPC<&ExampleService::RequestBidirectionalStreaming>;

using Channel = asio::experimental::channel<void(boost::system::error_code, example::v1::Request)>;

// This function will read one requests from the client at a time. Note that gRPC only allows calling agrpc::read after
// a previous read has completed.
asio::awaitable<void> reader(BidiStreamingRPC& rpc, Channel& channel)
{
    while (true)
    {
        example::v1::Request request;
        if (!co_await rpc.read(request))
        {
            // Client is done writing.
            break;
        }
        // Send request to writer. The `max_buffer_size` of the channel acts as backpressure.
        (void)co_await channel.async_send(boost::system::error_code{}, std::move(request),
                                          asio::as_tuple(asio::use_awaitable));
    }
    // Signal the writer to complete.
    channel.close();
}

// The writer will pick up reads from the reader through the channel and switch to the thread_pool to compute their
// response.
asio::awaitable<bool> writer(BidiStreamingRPC& rpc, Channel& channel, asio::thread_pool& thread_pool)
{
    bool ok{true};
    while (ok)
    {
        const auto [ec, request] = co_await channel.async_receive(asio::as_tuple(asio::use_awaitable));
        if (ec)
        {
            // Channel got closed by the reader.
            break;
        }
        // In this example we switch to the thread_pool to compute the response.
        co_await asio::post(asio::bind_executor(thread_pool, asio::use_awaitable));

        // Compute the response.
        example::v1::Response response;
        response.set_integer(request.integer() * 2);

        // rpc.write() is thread-safe so we can interact with it from the thread_pool.
        ok = co_await rpc.write(response);
        // Now we are back on the main thread.
    }
    co_return ok;
}

auto bidirectional_streaming_rpc_handler(asio::thread_pool& thread_pool)
{
    return [&](BidiStreamingRPC& rpc) -> asio::awaitable<void>
    {
        // Maximum number of requests that are buffered by the channel to enable backpressure.
        static constexpr auto MAX_BUFFER_SIZE = 2;

        Channel channel{co_await asio::this_coro::executor, MAX_BUFFER_SIZE};

        using namespace asio::experimental::awaitable_operators;
        const auto ok = co_await (reader(rpc, channel) && writer(rpc, channel, thread_pool));

        if (!ok)
        {
            // Client has disconnected or server is shutting down.
            co_return;
        }

        co_await rpc.finish(grpc::Status::OK);
    };
}
// ---------------------------------------------------
//

// The SlowUnary endpoint is used by the client to demonstrate per-RPC step cancellation. See streaming-client.cpp.
// It also demonstrates how to use an awaitable with a different executor type.
using SlowUnaryRPC = agrpc::ServerRPC<&ExampleExtService::RequestSlowUnary>;

asio::awaitable<void, agrpc::GrpcExecutor> handle_slow_unary_request(SlowUnaryRPC& rpc, SlowUnaryRPC::Request& request)
{
    agrpc::Alarm alarm{co_await asio::this_coro::executor};
    co_await alarm.wait(std::chrono::system_clock::now() + std::chrono::milliseconds(request.delay()));

    co_await rpc.finish({}, grpc::Status::OK);
}
// ---------------------------------------------------
//

using ShutdownRPC = agrpc::ServerRPC<&ExampleExtService::RequestShutdown>;

int main(int argc, const char** argv)
{
    const char* port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    ExampleService service;
    builder.RegisterService(&service);
    ExampleExtService service_ext;
    builder.RegisterService(&service_ext);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    example::ServerShutdown server_shutdown{*server, grpc_context};

    asio::thread_pool thread_pool{1};

    agrpc::register_awaitable_rpc_handler<ClientStreamingRPC>(grpc_context, service, &handle_client_streaming_request,
                                                              example::RethrowFirstArg{});

    agrpc::register_awaitable_rpc_handler<ServerStreamingRPC>(grpc_context, service, &handle_server_streaming_request,
                                                              example::RethrowFirstArg{});

    agrpc::register_awaitable_rpc_handler<ServerStreamingNotifyWhenDoneRPC>(
        grpc_context, service_ext, server_streaming_notify_when_done_request_handler(grpc_context),
        example::RethrowFirstArg{});

    agrpc::register_awaitable_rpc_handler<BidiStreamingRPC>(
        grpc_context, service, bidirectional_streaming_rpc_handler(thread_pool), example::RethrowFirstArg{});

    agrpc::register_awaitable_rpc_handler<SlowUnaryRPC>(grpc_context, service_ext, &handle_slow_unary_request,
                                                        example::RethrowFirstArg{});

    agrpc::register_awaitable_rpc_handler<ShutdownRPC>(
        grpc_context, service_ext,
        [&](ShutdownRPC& rpc, const ShutdownRPC::Request&) -> asio::awaitable<void>
        {
            if (co_await rpc.finish({}, grpc::Status::OK))
            {
                std::cout << "Received shutdown request from client\n";
                server_shutdown.shutdown();
            }
        },
        example::RethrowFirstArg{});

    grpc_context.run();
    std::cout << "Shutdown completed\n";
}