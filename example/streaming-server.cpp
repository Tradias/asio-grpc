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
#include "helper.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/signal_set.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>
#include <optional>
#include <thread>

struct ServerShutdown
{
    grpc::Server& server;
    boost::asio::basic_signal_set<agrpc::GrpcContext::executor_type> signals;
    std::optional<std::thread> shutdown_thread;

    ServerShutdown(grpc::Server& server, agrpc::GrpcContext& grpc_context)
        : server(server), signals(grpc_context, SIGINT, SIGTERM)
    {
        signals.async_wait(
            [&](auto&&, auto&&)
            {
                shutdown();
            });
    }

    void shutdown()
    {
        if (!shutdown_thread)
        {
            // This will cause all coroutines to run to completion normally
            // while returning `false` from RPC related steps, cancelling the signal
            // so that the GrpcContext will eventually run out of work and return
            // from `run()`.
            shutdown_thread.emplace(
                [&]
                {
                    signals.cancel();
                    server.Shutdown();
                });
            // Alternatively call `grpc_context.stop()` here instead which causes all coroutines
            // to end at their next suspension point.
            // Then call `server->Shutdown()` after the call to `grpc_context.run()` returns
            // or `.reset()` the grpc_context and go into another `grpc_context.run()`
        }
    }

    ~ServerShutdown()
    {
        if (shutdown_thread)
        {
            shutdown_thread->join();
        }
    }
};

boost::asio::awaitable<void> handle_client_streaming_request(
    grpc::ServerContext&, grpc::ServerAsyncReader<example::v1::Response, example::v1::Request>& reader)
{
    // Optionally send initial metadata first.
    bool send_ok = co_await agrpc::send_initial_metadata(reader);

    bool read_ok;
    do
    {
        example::v1::Request request;
        // Read from the client stream until the client has signaled `writes_done`.
        read_ok = co_await agrpc::read(reader, request);
    } while (read_ok);

    example::v1::Response response;
    bool finish_ok = co_await agrpc::finish(reader, response, grpc::Status::OK);

    // Or finish with an error
    // bool finish_with_error_ok = co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);

    // See documentation for the meaning of the bool values

    silence_unused(send_ok, finish_ok);
}

void register_client_streaming_handler(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    // Register a handler for all incoming RPCs of this method (Example::ClientStreaming) until the server is being
    // shut down. An API for requesting to handle a single RPC is also available:
    // `agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, services, server_context, request,
    // reader)`
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestClientStreaming, service,
        boost::asio::bind_executor(grpc_context.get_executor(), &handle_client_streaming_request));
}

boost::asio::awaitable<void> handle_bidirectional_streaming_request(example::v1::Example::AsyncService& service)
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

    // Let's perform a request/response ping-pong until the client is done sending requests,
    // incrementing an integer in the client's request each time.
    example::v1::Request request;
    bool read_ok = co_await agrpc::read(reader_writer, request);
    bool write_ok{true};
    while (read_ok && write_ok)
    {
        example::v1::Response response;
        response.set_integer(request.integer() + 1);
        // Reads and writes can be performed simultaneously.
        using namespace boost::asio::experimental::awaitable_operators;
        std::tie(read_ok, write_ok) =
            co_await(agrpc::read(reader_writer, request) && agrpc::write(reader_writer, response));
    }

    bool finish_ok = co_await agrpc::finish(reader_writer, grpc::Status::OK);

    silence_unused(finish_ok);
}

// This endpoint is used by the client to demonstrate per RPC step cancellation
boost::asio::awaitable<void> handle_slow_unary_request(example::v1::Example::AsyncService& service)
{
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestSlowUnary, service, server_context,
                                 request, writer))
    {
        co_return;
    }

    grpc::Alarm alarm;
    co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::milliseconds(request.integer()));

    example::v1::Response response;
    co_await agrpc::finish(writer, response, grpc::Status::OK);
}

// This helps with writing unit tests for these examples.
boost::asio::awaitable<void> handle_shutdown_request(example::v1::Example::AsyncService& service,
                                                     ServerShutdown& server_shutdown)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    google::protobuf::Empty request;
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestShutdown, service, server_context, request,
                                 writer))
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
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    ServerShutdown server_shutdown{*server, grpc_context};

    register_client_streaming_handler(service, grpc_context);
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await handle_bidirectional_streaming_request(service);
        },
        boost::asio::detached);
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await handle_slow_unary_request(service);
        },
        boost::asio::detached);
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await handle_shutdown_request(service, server_shutdown);
        },
        boost::asio::detached);

    grpc_context.run();
    std::cout << "Shutdown completed\n";
}