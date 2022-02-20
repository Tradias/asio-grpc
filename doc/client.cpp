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
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

namespace asio = boost::asio;

asio::awaitable<void> unary(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [request-unary-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        stub.AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
    /* [request-unary-client-side] */

    // begin-snippet: unary-client-side
    bool read_ok = co_await agrpc::read_initial_metadata(*reader, asio::use_awaitable);

    example::v1::Response response;
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader, response, status, asio::use_awaitable);
    // end-snippet

    silence_unused(read_ok, finish_ok);
}

asio::awaitable<void> unary_awaitable(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [request-unary-client-side-await] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        co_await agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context, request);
    /* [request-unary-client-side-await] */
}

asio::awaitable<void> client_streaming(example::v1::Example::Stub& stub)
{
    /* [request-client-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Response response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context,
                                              writer, response, asio::use_awaitable);
    /* [request-client-streaming-client-side] */

    // begin-snippet: client-streaming-client-side
    bool read_ok = co_await agrpc::read_initial_metadata(*writer, asio::use_awaitable);

    example::v1::Request request;
    bool write_ok = co_await agrpc::write(*writer, request, asio::use_awaitable);

    bool writes_done_ok = co_await agrpc::writes_done(*writer, asio::use_awaitable);

    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*writer, status, asio::use_awaitable);
    // end-snippet

    silence_unused(request_ok, read_ok, write_ok, writes_done_ok, finish_ok);
}

asio::awaitable<void> client_streaming_alt(example::v1::Example::Stub& stub)
{
    /* [request-client-streaming-client-side-alt] */
    grpc::ClientContext client_context;
    example::v1::Response response;
    auto [writer, request_ok] = co_await agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub,
                                                        client_context, response, asio::use_awaitable);
    /* [request-client-streaming-client-side-alt] */

    silence_unused(writer, request_ok);
}

void client_streaming_corked(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    /* [request-client-streaming-client-side-corked] */
    grpc::ClientContext client_context;
    client_context.set_initial_metadata_corked(true);
    example::v1::Response response;
    auto writer =
        stub.AsyncClientStreaming(&client_context, &response, agrpc::get_completion_queue(grpc_context), nullptr);
    /* [request-client-streaming-client-side-corked] */

    silence_unused(writer);
}

asio::awaitable<void> server_streaming(example::v1::Example::Stub& stub)
{
    /* [request-server-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context,
                                              request, reader, asio::use_awaitable);
    /* [request-server-streaming-client-side] */

    // begin-snippet: server-streaming-client-side
    bool read_metadata_ok = co_await agrpc::read_initial_metadata(*reader, asio::use_awaitable);

    example::v1::Response response;
    bool read_ok = co_await agrpc::read(*reader, response, asio::use_awaitable);

    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader, status, asio::use_awaitable);
    // end-snippet

    silence_unused(request_ok, read_metadata_ok, read_ok, finish_ok);
}

asio::awaitable<void> server_streaming_alt(example::v1::Example::Stub& stub)
{
    /* [request-server-streaming-client-side-alt] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    auto [reader, request_ok] = co_await agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub,
                                                        client_context, request, asio::use_awaitable);
    /* [request-server-streaming-client-side-alt] */

    silence_unused(reader, request_ok);
}

asio::awaitable<void> bidirectional_streaming(example::v1::Example::Stub& stub)
{
    /* [request-bidirectional-client-side] */
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub,
                                              client_context, reader_writer, asio::use_awaitable);
    /* [request-bidirectional-client-side] */

    // begin-snippet: bidirectional-client-side
    bool read_metadata_ok = co_await agrpc::read_initial_metadata(*reader_writer, asio::use_awaitable);

    example::v1::Request request;
    bool write_ok = co_await agrpc::write(*reader_writer, request, asio::use_awaitable);

    bool writes_done_ok = co_await agrpc::writes_done(*reader_writer, asio::use_awaitable);

    example::v1::Response response;
    bool read_ok = co_await agrpc::read(*reader_writer, response, asio::use_awaitable);

    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader_writer, status, asio::use_awaitable);
    // end-snippet

    silence_unused(request_ok, read_metadata_ok, write_ok, writes_done_ok, read_ok, finish_ok);
}

asio::awaitable<void> bidirectional_streaming_alt(example::v1::Example::Stub& stub)
{
    /* [request-bidirectional-client-side-alt] */
    grpc::ClientContext client_context;
    auto [reader_writer, request_ok] = co_await agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming,
                                                               stub, client_context, asio::use_awaitable);
    /* [request-bidirectional-client-side-alt] */

    silence_unused(reader_writer, request_ok);
}

void bidirectional_streaming_corked(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    /* [request-client-bidirectional-client-side-corked] */
    grpc::ClientContext client_context;
    client_context.set_initial_metadata_corked(true);
    auto reader_writer =
        stub.AsyncBidirectionalStreaming(&client_context, agrpc::get_completion_queue(grpc_context), nullptr);
    /* [request-client-bidirectional-client-side-corked] */
}

int main()
{
    auto stub =
        example::v1::Example::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    // begin-snippet: create-grpc_context-client-side
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    // end-snippet

    // begin-snippet: make-work-guard
    auto guard = asio::make_work_guard(grpc_context);
    // end-snippet
    asio::co_spawn(
        grpc_context,
        [&]()
        {
            return unary(grpc_context, *stub);
        },
        asio::detached);

    grpc_context.run();
}