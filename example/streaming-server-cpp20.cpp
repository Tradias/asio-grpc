// Copyright 2021 Dennis Hezel
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

#include "coSpawner.hpp"
#include "helper.hpp"
#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
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
            shutdown_thread.emplace(
                [&]
                {
                    signals.cancel();
                    server.Shutdown();
                });
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
    // Optionally send initial metadata
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_async_reader.html#a3158928c9f07d695b3deeb39e5dd0982
    bool send_ok = co_await agrpc::send_initial_metadata(reader);

    bool read_ok;
    do
    {
        example::v1::Request request;
        // Read from the client stream
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_async_reader.html#aefbe83b79433a6ae85f9f1ce913997c1
        read_ok = co_await agrpc::read(reader, request);
    } while (read_ok);

    // See also https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_async_reader.html#a59951d06481769e3e40fc84454901467
    example::v1::Response response;
    bool finish_ok = co_await agrpc::finish(reader, response, grpc::Status::OK);

    // Or finish with an error
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_async_reader.html#ad35cb525d0eeaf49e61bd5a5aeaf9a05
    // bool finish_with_error_ok = co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);

    // See https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a
    // for the meaning of the bool values

    silence_unused(send_ok, finish_ok);
}

void register_client_streaming_handler(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    // Optionally register our handler so that it will handle all incoming
    // requests for this RPC method (Example::ClientStreaming) until the server is being shut down.
    // An API for requesting to handle a single RPC is also available:
    // `agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, services, server_context, request,
    // *reader)`
    //
    // Note that this is an experimental feature which means that it works correctly but its
    // API is still subject to breaking changes
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestClientStreaming, service,
        CoSpawner{boost::asio::bind_executor(grpc_context,
                                             [&](auto& server_context, auto& reader) -> boost::asio::awaitable<void>
                                             {
                                                 co_await handle_client_streaming_request(server_context, reader);
                                             })});
}

boost::asio::awaitable<void> handle_bidirectional_streaming_request(example::v1::Example::AsyncService& service,
                                                                    agrpc::GrpcContext& grpc_context)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request> reader_writer{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestBidirectionalStreaming,
                                              service, server_context, reader_writer);
    if (!request_ok)
    {
        // Server is shutting down.
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a
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
        using namespace boost::asio::experimental::awaitable_operators;
        // Reads and writes can be done simultaneously.
        // More information on reads:
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_async_reader_writer.html#a911d41783f644e170c017bb3032ca5c8
        // More information on writes:
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_async_reader_writer.html#a816fd9065473a24d4ab613ee2ecb19e1
        std::tie(read_ok, write_ok) =
            co_await(agrpc::read(reader_writer, request) && agrpc::write(reader_writer, response));
    }

    bool finish_ok = co_await agrpc::finish(reader_writer, grpc::Status::OK);

    silence_unused(finish_ok);
}

boost::asio::awaitable<void> handle_shutdown_request(example::v1::Example::AsyncService& service,
                                                     agrpc::GrpcContext& grpc_context, ServerShutdown& server_shutdown)
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

    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    example::v1::Example::AsyncService service;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(std::string("0.0.0.0:") + port, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    ServerShutdown server_shutdown{*server, grpc_context};

    register_client_streaming_handler(service, grpc_context);
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await handle_bidirectional_streaming_request(service, grpc_context);
        },
        boost::asio::detached);
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await handle_shutdown_request(service, grpc_context, server_shutdown);
        },
        boost::asio::detached);

    grpc_context.run();
    std::cout << "Shutdown completed\n";
}