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
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <iostream>

boost::asio::awaitable<void> make_client_streaming_request(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

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

    // Signal that we are done writing
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_writer.html#ab9fd5cd7dedd73d878a86de84c58e8ff
    bool writes_done_ok = co_await agrpc::writes_done(*writer);

    // Wait for the server to recieve all our messages
    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_writer.html#aa94f32feaf8200695b65f1628d0ddf50
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*writer, status);

    // See https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a
    // for the meaning of the bool values

    abort_if_not(status.ok());
    silence_unused(request_ok, read_ok, write_ok, writes_done_ok, finish_ok);
}

boost::asio::awaitable<void> make_bidirectional_streaming_request(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub,
                                              client_context, reader_writer);
    if (!request_ok)
    {
        // See 'Client-side StartCall/RPC invocation' for an explanation when `request_ok` can be false
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html#a86d9810ced694e50f7987ac90b9f8c1a
        co_return;
    }

    // Let's perform a request/response ping-pong.
    example::v1::Request request;
    request.set_integer(0);
    bool write_ok{true};
    bool read_ok{true};
    int count{};
    while (read_ok && write_ok && count < 10)
    {
        example::v1::Response response;
        // Reads and writes can be done simultaneously.
        // More information on reads:
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_reader_writer.html#a2f2c91734a7de5affdd427fb88ee6167
        // More information on writes:
        // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_reader_writer.html#a07bcc3a0737c3bee3ef50def89af5fc7
        using namespace boost::asio::experimental::awaitable_operators;
        std::tie(read_ok, write_ok) =
            co_await(agrpc::read(*reader_writer, response) && agrpc::write(*reader_writer, request));
        std::cout << "Bidirectional streaming: " << response.integer() << '\n';
        request.set_integer(response.integer());
        ++count;
    }
    co_await agrpc::writes_done(*reader_writer);

    // https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_async_reader_writer.html#a953d61a2b25473bb76c7038b940b87aa
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader_writer, status);

    abort_if_not(status.ok());
    silence_unused(finish_ok);
}

template <class Function>
boost::asio::awaitable<void> run_with_deadline(grpc::Alarm& alarm, const boost::asio::any_io_executor& executor,
                                               grpc::ClientContext& client_context,
                                               std::chrono::system_clock::time_point deadline, Function&& function)
{
    const auto set_alarm = [&]() -> boost::asio::awaitable<void>
    {
        if (co_await agrpc::wait(alarm, deadline))
        {
            client_context.TryCancel();
        }
    };
    using namespace boost::asio::experimental::awaitable_operators;
    co_await(set_alarm() || function());
}

boost::asio::awaitable<void> make_and_cancel_unary_request(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    const auto executor = co_await boost::asio::this_coro::executor;

    example::v1::Request request;
    request.set_integer(2000);  // tell server to delay response by 2000ms
    auto reader = stub.AsyncSlowUnary(&client_context, request, agrpc::get_completion_queue(executor));

    example::v1::Response response;
    grpc::Status status;
    grpc::Alarm alarm;
    co_await run_with_deadline(alarm, executor, client_context,
                               std::chrono::system_clock::now() + std::chrono::milliseconds(50),
                               [&]
                               {
                                   return agrpc::finish(*reader, response, status);
                               });

    abort_if_not(grpc::StatusCode::CANCELLED == status.error_code());
}

boost::asio::awaitable<void> make_shutdown_request(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    google::protobuf::Empty request;
    const auto reader =
        co_await agrpc::request(&example::v1::Example::Stub::AsyncShutdown, stub, client_context, request);

    google::protobuf::Empty response;
    grpc::Status status;
    if (co_await agrpc::finish(*reader, response, status) && status.ok())
    {
        std::cout << "Successfully send shutdown request to server\n";
    }
    else
    {
        std::cout << "Failed to send shutdown request to server: " << status.error_message() << '\n';
    }
    abort_if_not(status.ok());
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto stub = example::v1::Example::NewStub(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            // Let's perform the client-streaming and bidirectional-streaming requests simultaneously
            using namespace boost::asio::experimental::awaitable_operators;
            co_await(make_client_streaming_request(*stub) && make_bidirectional_streaming_request(*stub));
            co_await make_and_cancel_unary_request(*stub);
            co_await make_shutdown_request(*stub);
        },
        boost::asio::detached);

    grpc_context.run();
}