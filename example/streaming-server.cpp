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
#include "server_shutdown_asio.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/thread_pool.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>
#include <thread>

namespace asio = boost::asio;

// Example showing some of the features of using asio-grpc with Boost.Asio.

// begin-snippet: server-side-client-streaming
// ---------------------------------------------------
// A simple client-streaming request handler using coroutines.
// ---------------------------------------------------
// end-snippet
asio::awaitable<void> handle_client_streaming_request(
    grpc::ServerContext&, grpc::ServerAsyncReader<example::v1::Response, example::v1::Request>& reader)
{
    // Optionally send initial metadata first.
    // Since the default completion token in asio-grpc is asio::use_awaitable, this line is equivalent to:
    // co_await agrpc::send_initial_metadata(reader, asio::use_awaitable);
    bool send_ok = co_await agrpc::send_initial_metadata(reader);

    bool read_ok;
    do
    {
        example::v1::Request request;
        // Read from the client stream until the client has signaled `writes_done`.
        read_ok = co_await agrpc::read(reader, request);
    } while (read_ok);

    example::v1::Response response;
    co_await agrpc::finish(reader, response, grpc::Status::OK);

    // Or finish with an error
    // co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);

    // See documentation for the meaning of the bool values

    silence_unused(send_ok);
}

void register_client_streaming_handler(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    // Register a handler for all incoming RPCs of this method (Example::ClientStreaming) until the server is being
    // shut down. An API for requesting to handle a single RPC is also available:
    // `agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service, server_context, reader)`
    agrpc::repeatedly_request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
                              asio::bind_executor(grpc_context, &handle_client_streaming_request));
}
// ---------------------------------------------------
//

// begin-snippet: server-side-server-streaming
// ---------------------------------------------------
// A simple server-streaming request handler using coroutines.
// ---------------------------------------------------
// end-snippet
void register_server_streaming_handler(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    auto request_handler = [](grpc::ServerContext&, example::v1::Request& request,
                              grpc::ServerAsyncWriter<example::v1::Response>& writer) -> asio::awaitable<void>
    {
        example::v1::Response response;
        response.set_integer(request.integer());
        while (co_await agrpc::write(writer, response) && response.integer() > 0)
        {
            response.set_integer(response.integer() - 1);
        }
        co_await agrpc::finish(writer, grpc::Status::OK);
    };

    // Register a handler for all incoming RPCs of this method (Example::ServerStreaming) until the server is being
    // shut down.
    agrpc::repeatedly_request(&example::v1::Example::AsyncService::RequestServerStreaming, service,
                              asio::bind_executor(grpc_context, request_handler));
}
// ---------------------------------------------------
//

// begin-snippet: server-side-bidirectional-streaming
// ---------------------------------------------------
// The following bidirectional-streaming example shows how to dispatch requests to a thread_pool and write responses
// back to the client.
// ---------------------------------------------------
// end-snippet
using Channel = asio::experimental::channel<void(boost::system::error_code, example::v1::Request)>;

// This function will read one requests from the client at a time. Note that gRPC only allows calling agrpc::read after
// a previous read has completed.
asio::awaitable<void> reader(grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request>& reader_writer,
                             Channel& channel)
{
    while (true)
    {
        example::v1::Request request;
        if (!co_await agrpc::read(reader_writer, request))
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
asio::awaitable<bool> writer(grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request>& reader_writer,
                             Channel& channel, asio::thread_pool& thread_pool)
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

        // reader_writer is thread-safe so we can just interact with it from the thread_pool.
        ok = co_await agrpc::write(reader_writer, response);
        // Now we are back on the main thread.
    }
    co_return ok;
}

asio::awaitable<void> handle_bidirectional_streaming_request(example::v1::Example::AsyncService& service,
                                                             asio::thread_pool& thread_pool)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request> reader_writer{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestBidirectionalStreaming,
                                              service, server_context, reader_writer);
    if (!request_ok)
    {
        // Server is shutting down.
        co_return;
    }

    // Maximum number of requests that are buffered by the channel to enable backpressure.
    static constexpr auto MAX_BUFFER_SIZE = 2;

    Channel channel{co_await asio::this_coro::executor, MAX_BUFFER_SIZE};

    using namespace asio::experimental::awaitable_operators;
    const auto ok = co_await (reader(reader_writer, channel) && writer(reader_writer, channel, thread_pool));

    if (!ok)
    {
        // Client has disconnected or server is shutting down.
        co_return;
    }

    co_await agrpc::finish(reader_writer, grpc::Status::OK);
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The SlowUnary endpoint is used by the client to demonstrate per-RPC step cancellation. See streaming-client.cpp.
// ---------------------------------------------------
asio::awaitable<void> handle_slow_unary_request(example::v1::ExampleExt::AsyncService& service)
{
    grpc::ServerContext server_context;
    example::v1::SlowRequest request;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    if (!co_await agrpc::request(&example::v1::ExampleExt::AsyncService::RequestSlowUnary, service, server_context,
                                 request, writer))
    {
        co_return;
    }

    grpc::Alarm alarm;
    co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::milliseconds(request.delay()));

    co_await agrpc::finish(writer, {}, grpc::Status::OK);
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The Shutdown endpoint is used by unit tests.
// ---------------------------------------------------
asio::awaitable<void> handle_shutdown_request(example::v1::ExampleExt::AsyncService& service,
                                              example::ServerShutdown& server_shutdown)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    google::protobuf::Empty request;
    if (!co_await agrpc::request(&example::v1::ExampleExt::AsyncService::RequestShutdown, service, server_context,
                                 request, writer))
    {
        co_return;
    }

    google::protobuf::Empty response;
    if (co_await agrpc::finish(writer, response, grpc::Status::OK))
    {
        std::cout << "Received shutdown request from client\n";
        server_shutdown.shutdown();
    }
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    example::v1::Example::AsyncService service;
    builder.RegisterService(&service);
    example::v1::ExampleExt::AsyncService service_ext;
    builder.RegisterService(&service_ext);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    example::ServerShutdown server_shutdown{*server, grpc_context};

    asio::thread_pool thread_pool{1};

    register_client_streaming_handler(grpc_context, service);
    register_server_streaming_handler(grpc_context, service);
    asio::co_spawn(grpc_context, handle_bidirectional_streaming_request(service, thread_pool), asio::detached);
    asio::co_spawn(grpc_context, handle_slow_unary_request(service_ext), asio::detached);
    asio::co_spawn(grpc_context, handle_shutdown_request(service_ext, server_shutdown), asio::detached);

    grpc_context.run();
    std::cout << "Shutdown completed\n";
}