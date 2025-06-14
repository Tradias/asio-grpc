// Copyright 2023 Dennis Hezel
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
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/spawn.hpp>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/generic_stub.h>

#include <functional>
#include <iostream>

namespace asio = boost::asio;

// Example showing how to write a generic client for a unary and a bidirectional streaming RPC.

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

// begin-snippet: client-side-generic-unary-request
// ---------------------------------------------------
// A simple generic unary with Boost.Coroutine.
// ---------------------------------------------------
// end-snippet
void make_generic_unary_request(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub,
                                const asio::yield_context& yield)
{
    using RPC = agrpc::GenericUnaryClientRPC;

    example::v1::Request request;
    request.set_integer(1);

    // -- Serialize the request message
    auto request_buffer = serialize(request);

    // -- Initiate the unary request:
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    grpc::ByteBuffer response_buffer;
    auto status = RPC::request(grpc_context, "/example.v1.Example/Unary", stub, client_context, request_buffer,
                               response_buffer, yield);

    abort_if_not(status.ok());

    // -- For streaming RPCs use:
    // agrpc::ClientRPC<agrpc::CLIENT_GENERIC_STREAMING_RPC>::request(grpc_context,
    // "/example.v1.Example/ServerStreaming", stub, client_context, yield);

    // -- Deserialize the response message
    example::v1::Response response;
    abort_if_not(deserialize(response_buffer, response));
    abort_if_not(2 == response.integer());
}
// ---------------------------------------------------
//

// begin-snippet: client-side-generic-bidirectional-request
// ---------------------------------------------------
// A generic bidirectional-streaming request that simply sends the response from the server back to it using Asio's
// stackless coroutines.
// ---------------------------------------------------
// end-snippet
struct BidirectionalStreamingRequest
{
    struct Context
    {
        grpc::GenericStub& stub;
        agrpc::GenericStreamingClientRPC rpc;
        example::v1::Request request;
        grpc::ByteBuffer response_buffer;
        int count{};
        bool write_ok{true};
        bool read_ok{true};

        Context(agrpc::GrpcContext& grpc_context, grpc::GenericStub& generic_stub)
            : stub(generic_stub), rpc(grpc_context)
        {
        }
    };

    std::unique_ptr<Context> context;
    asio::coroutine coro;

    BidirectionalStreamingRequest(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub)
        : context(std::make_unique<Context>(grpc_context, stub))
    {
    }

    template <class Self>
    void operator()(Self& self, const std::array<std::size_t, 2>&, bool read_ok, bool write_ok)
    {
        operator()(self, read_ok, write_ok);
    }

    template <class Self>
    void operator()(Self& self, const grpc::Status& status)
    {
        operator()(self, {}, {}, status);
    }

    template <class Self>
    void operator()(Self& self, bool ok = {}, bool write_ok = {}, const grpc::Status& status = grpc::Status::OK)
    {
        auto& c = *context;
        BOOST_ASIO_CORO_REENTER(coro)
        {
            c.rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

            BOOST_ASIO_CORO_YIELD c.rpc.start("/example.v1.Example/BidirectionalStreaming", c.stub, std::move(self));

            if (!ok)
            {
                // Channel is either permanently broken or transiently broken but with the fail-fast option.
                return;
            }

            // Let's perform a request/response ping-pong.
            c.request.set_integer(1);
            while (c.read_ok && c.write_ok && c.count < 10)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    auto request_buffer = serialize(c.request);

                    // Reads and writes can be performed simultaneously.
                    asio::experimental::make_parallel_group(
                        [&](auto&& token)
                        {
                            return c.rpc.read(c.response_buffer, std::move(token));
                        },
                        [&](auto&& token)
                        {
                            return c.rpc.write(request_buffer, std::move(token));
                        })
                        .async_wait(asio::experimental::wait_for_all(), std::move(self));
                }
                c.read_ok = ok;
                c.write_ok = write_ok;

                example::v1::Response response;
                abort_if_not(deserialize(c.response_buffer, response));

                std::cout << "Generic: bidirectional streaming: " << response.integer() << '\n';
                c.request.set_integer(response.integer());
                ++c.count;
            }

            // Do not forget to signal that we are done writing before finishing.
            BOOST_ASIO_CORO_YIELD c.rpc.writes_done(std::move(self));

            BOOST_ASIO_CORO_YIELD c.rpc.finish(std::move(self));

            abort_if_not(status.ok());

            context.reset();
            self.complete();
        }
    }
};

template <class CompletionToken>
auto make_bidirectional_streaming_request(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub,
                                          CompletionToken&& token)
{
    return asio::async_compose<CompletionToken, void()>(BidirectionalStreamingRequest{grpc_context, stub}, token,
                                                        grpc_context);
}
// ---------------------------------------------------
//

void make_shutdown_request(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::Stub& stub)
{
    struct Context
    {
        grpc::ClientContext client_context;
        google::protobuf::Empty response;
    };
    auto context = std::make_unique<Context>();
    auto& [client_context, response] = *context;
    agrpc::ClientRPC<&example::v1::ExampleExt::Stub::PrepareAsyncShutdown>::request(
        grpc_context, stub, client_context, {}, response,
        [c = std::move(context)](const grpc::Status& status)
        {
            abort_if_not(status.ok());
        });
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());

    grpc::GenericStub generic_stub{channel};

    // We can mix generic and non-generic requests on the same channel.
    example::v1::ExampleExt::Stub stub{channel};

    agrpc::GrpcContext grpc_context;

    example::spawn(grpc_context,
                   [&](asio::yield_context yield)
                   {
                       // First we perform the unary request using Boost.Coroutine
                       make_generic_unary_request(grpc_context, generic_stub, yield);
                       // Then we do the bidirectional streaming request using stackless coroutines.
                       make_bidirectional_streaming_request(grpc_context, generic_stub,
                                                            [&]
                                                            {
                                                                // And finally the shutdown request using
                                                                // callbacks.
                                                                make_shutdown_request(grpc_context, stub);
                                                            });
                   });

    grpc_context.run();
}