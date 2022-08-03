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
#include "yield_helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/spawn.hpp>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/generic_stub.h>

#include <iostream>

namespace asio = boost::asio;

// Example showing how to write to generic client that sends a single unary request.

template <class Message>
auto serialize(const Message& message)
{
    grpc::ByteBuffer buffer;
    bool own_buffer;
    grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Request>(message, &buffer, &own_buffer);
    return buffer;
}

template <class Message>
bool deserialize(grpc::ByteBuffer& buffer, Message& message)
{
    return grpc::GenericDeserialize<grpc::ProtoBufferReader, example::v1::Response>(&buffer, &message).ok();
}

void make_generic_unary_request(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub,
                                const asio::yield_context& yield)
{
    example::v1::Request request;
    request.set_integer(1);

    // -- Serialize the request message
    auto buffer = serialize(request);

    // -- Initiate the unary request:
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    const auto response_writer =
        agrpc::request("/example.v1.Example/Unary", stub, client_context, buffer, grpc_context);

    // -- For streaming RPC use:
    // std::unique_ptr<grpc::GenericClientAsyncReaderWriter> reader_writer;
    // agrpc::request("/example.v1.Example/ServerStreaming", stub, client_context, reader_writer, yield);

    // -- Wait for the response message
    buffer.Clear();
    grpc::Status status;
    agrpc::finish(response_writer, buffer, status, yield);
    abort_if_not(status.ok());

    // -- Deserialize the response message
    example::v1::Response response;
    abort_if_not(deserialize(buffer, response));
    abort_if_not(2 == response.integer());
}

void make_bidirectional_streaming_request(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub,
                                          const asio::yield_context& yield)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> reader_writer;
    bool request_ok =
        agrpc::request("/example.v1.Example/BidirectionalStreaming", stub, client_context, reader_writer, yield);
    if (!request_ok)
    {
        // Channel is either permanently broken or transiently broken but with the fail-fast option.
        return;
    }

    // Let's perform a request/response ping-pong.
    example::v1::Request request;
    request.set_integer(1);
    bool write_ok{true};
    bool read_ok{true};
    int count{};
    while (read_ok && write_ok && count < 10)
    {
        auto request_buffer = serialize(request);

        example::v1::Response response;
        grpc::ByteBuffer response_buffer;

        // Reads and writes can be performed simultaneously.
        example::yield_when_all(
            grpc_context, yield,
            [&](bool is_read_ok, bool is_write_ok)
            {
                read_ok = is_read_ok;
                write_ok = is_write_ok;
            },
            [&](auto&& token)
            {
                return agrpc::read(reader_writer, response_buffer, std::move(token));
            },
            [&](auto&& token)
            {
                return agrpc::write(reader_writer, request_buffer, std::move(token));
            });

        abort_if_not(deserialize(response_buffer, response));

        std::cout << "Generic: bidirectional streaming: " << response.integer() << '\n';
        request.set_integer(response.integer());
        ++count;
    }

    // Do not forget to signal that we are done writing before finishing.
    agrpc::writes_done(reader_writer, yield);

    grpc::Status status;
    agrpc::finish(reader_writer, status, yield);

    abort_if_not(status.ok());
}

void make_shutdown_request(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::Stub& stub,
                           const asio::yield_context& yield)
{
    grpc::ClientContext client_context;
    const auto reader = stub.AsyncShutdown(&client_context, {}, agrpc::get_completion_queue(grpc_context));
    google::protobuf::Empty response;
    grpc::Status status;
    agrpc::finish(*reader, response, status, yield);
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());

    grpc::GenericStub generic_stub{channel};

    // We can mix generic and non-generic requests on the same channel.
    example::v1::ExampleExt::Stub stub{channel};

    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    asio::spawn(grpc_context,
                [&](asio::yield_context yield)
                {
                    make_generic_unary_request(grpc_context, generic_stub, yield);
                    make_bidirectional_streaming_request(grpc_context, generic_stub, yield);
                    make_shutdown_request(grpc_context, stub, yield);
                });

    grpc_context.run();
}