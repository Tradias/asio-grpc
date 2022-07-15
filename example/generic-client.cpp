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
#include <boost/asio/spawn.hpp>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/generic_stub.h>

namespace asio = boost::asio;

// Example showing how to write to generic client that sends a single unary request.

void make_generic_unary_request(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub,
                                const asio::yield_context& yield)
{
    example::v1::Request request;
    request.set_integer(42);

    // -- Serialize the request message
    grpc::ByteBuffer buffer;
    bool own_buffer;
    grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Request>(request, &buffer, &own_buffer);

    // -- Initiate the unary request:
    grpc::ClientContext client_context;
    const auto response_writer = agrpc::request("/test.v1.Test/Unary", stub, client_context, buffer, grpc_context);

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
    const auto deserialize_status =
        grpc::GenericDeserialize<grpc::ProtoBufferReader, example::v1::Response>(&buffer, &response);
    abort_if_not(deserialize_status.ok());
    abort_if_not(21 == response.integer());
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
                    make_shutdown_request(grpc_context, stub, yield);
                });

    grpc_context.run();
}