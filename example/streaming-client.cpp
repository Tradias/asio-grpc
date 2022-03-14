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
#include "example/v1/exampleExt.grpc.pb.h"
#include "helper.hpp"

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

    // Optionally read initial metadata first.
    bool read_ok = co_await agrpc::read_initial_metadata(*writer);

    // Send a message.
    example::v1::Request request;
    bool write_ok = co_await agrpc::write(*writer, request);

    // Signal that we are done writing.
    bool writes_done_ok = co_await agrpc::writes_done(*writer);

    // Wait for the server to recieve all our messages.
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*writer, status);

    // The above three steps can also be combined into one using `agrpc::write_and_finish`.

    // See documentation for the meaning of the bool values.

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
        // Channel is either permanently broken or transiently broken but with the fail-fast option.
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
        // Reads and writes can be performed simultaneously.
        using namespace boost::asio::experimental::awaitable_operators;
        std::tie(read_ok, write_ok) =
            co_await(agrpc::read(*reader_writer, response) && agrpc::write(*reader_writer, request));
        std::cout << "Bidirectional streaming: " << response.integer() << '\n';
        request.set_integer(response.integer());
        ++count;
    }

    // Do not forget to signal that we are done writing before finishing.
    co_await agrpc::writes_done(*reader_writer);

    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader_writer, status);

    abort_if_not(status.ok());
    silence_unused(finish_ok);
}

template <class Function>
boost::asio::awaitable<void> run_with_deadline(grpc::Alarm& alarm, grpc::ClientContext& client_context,
                                               std::chrono::system_clock::time_point deadline, Function function)
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

// This example shows how to cancel an entire RPC when a single step did not complete within the specified time.
boost::asio::awaitable<void> make_and_cancel_unary_request(example::v1::ExampleExt::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::SlowRequest request;
    request.set_delay(2000);  // tell server to delay response by 2000ms
    auto reader = stub.AsyncSlowUnary(&client_context, request, co_await agrpc::get_completion_queue());

    google::protobuf::Empty response;
    grpc::Status status;
    grpc::Alarm alarm;
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
    co_await run_with_deadline(alarm, client_context, std::chrono::system_clock::now() + std::chrono::milliseconds(100),
                               [&]
                               {
                                   return agrpc::finish(*reader, response, status);
                               });

    abort_if_not(std::chrono::steady_clock::now() < not_too_exceed);
    abort_if_not(grpc::StatusCode::CANCELLED == status.error_code());
}

// This helps with writing unit tests for these examples.
boost::asio::awaitable<void> make_shutdown_request(example::v1::ExampleExt::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    google::protobuf::Empty request;
    const auto reader =
        co_await agrpc::request(&example::v1::ExampleExt::Stub::AsyncShutdown, stub, client_context, request);

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

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto stub = example::v1::Example::NewStub(channel);
    const auto stub_ext = example::v1::ExampleExt::NewStub(channel);
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            // Let's perform the client-streaming and bidirectional-streaming requests simultaneously
            using namespace boost::asio::experimental::awaitable_operators;
            co_await(make_client_streaming_request(*stub) && make_bidirectional_streaming_request(*stub));
            co_await make_and_cancel_unary_request(*stub_ext);
            co_await make_shutdown_request(*stub_ext);
        },
        boost::asio::detached);

    grpc_context.run();
}