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

#include "helper.hpp"
#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

boost::asio::awaitable<void> make_client_streaming_request(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    example::v1::Response response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context,
                                              writer, response);

    // Optionally read initial metadata first
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_writer.html#a02f613eaaed4177f1682da3f5eebbdeb
    bool read_ok = co_await agrpc::read_initial_metadata(*writer);

    // Send a message
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_writer.html#a0e981b6dcce663e655edf305573f2f30
    example::v1::Request request;
    bool write_ok = co_await agrpc::write(*writer, request);

    // Optionally signal that we are done writing
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_writer.html#ab9fd5cd7dedd73d878a86de84c58e8ff
    bool writes_done_ok = co_await agrpc::writes_done(*writer);

    // Wait for the server to recieve all our messages
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_writer.html#aa94f32feaf8200695b65f1628d0ddf50
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*writer, status);

    // See https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a
    // for the meaning of the bool values

    silence_unused(request_ok, read_ok, write_ok, writes_done_ok, finish_ok);
}

int main()
{
    const auto stub =
        example::v1::Example::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            co_await make_client_streaming_request(*stub);
        },
        boost::asio::detached);

    grpc_context.run();
}