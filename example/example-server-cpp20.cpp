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
#include <boost/asio/signal_set.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <optional>
#include <thread>

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

    silence_unused(send_ok, read_ok, finish_ok);
}

void register_client_streaming_handler(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    // Optionally register our handler so that it will handle all incoming
    // requests for this RPC method (Example::ClientStreaming) until the server is being shut down.
    // An API for requesting to handle a single RPC is also available:
    // `agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, services, server_context, request,
    // *reader)`
    //
    // Note this is an experimental feature, which means that its
    // API is still subject to breaking changing
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestClientStreaming, service,
        CoSpawner{boost::asio::bind_executor(grpc_context,
                                             [&](auto& server_context, auto& reader) -> boost::asio::awaitable<void>
                                             {
                                                 co_await handle_client_streaming_request(server_context, reader);
                                             })});
}

int main()
{
    std::optional<std::thread> shutdown_thread;

    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    example::v1::Example::AsyncService service;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    boost::asio::basic_signal_set signals{grpc_context, SIGINT, SIGTERM};
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    register_client_streaming_handler(service, grpc_context);

    signals.async_wait(
        [&](auto&&, auto&&)
        {
            shutdown_thread.emplace(
                [&]
                {
                    server->Shutdown();
                });
        });

    grpc_context.run();
    shutdown_thread->join();
}