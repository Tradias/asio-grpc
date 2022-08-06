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

#include <agrpc/repeatedly_request.hpp>
#include <agrpc/rpc.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

// client
asio::awaitable<void> unary_rpc(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [full-unary-client-side] */
    grpc::ClientContext client_context;

    // Always set a deadline.
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context, request, grpc_context);

    // Optional step to retrieve initial metadata immediately.
    // Returns `false` if the call is dead. Use `agrpc::finish` to get a `grpc::Status` with error details.
    bool read_ok = co_await agrpc::read_initial_metadata(reader);

    example::v1::Response response;
    grpc::Status status;
    co_await agrpc::finish(reader, response, status);
    // If `status.ok()` then the server has sent its response.
    /* [full-unary-client-side] */

    silence_unused(read_ok);
}

asio::awaitable<void> client_streaming_rpc(example::v1::Example::Stub& stub)
{
    /* [full-client-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Response response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;

    // Returns `false` if there is a connection issue. Use `agrpc::finish` to get a `grpc::Status` with
    // error details.
    co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncClientStreaming, stub, client_context, writer,
                            response);

    example::v1::Request request;

    // Only one write may be outstanding at a time.
    // Returns `false` if the call is dead.
    co_await agrpc::write(writer, request);

    // There is also an overload that takes `grpc::WriteOptions`.
    // co_await agrpc::write(writer, grpc::WriteOptions{}, request)

    // Call `writes_done` when done sending messages to the server .
    // Returns `false` if the call is dead.
    co_await agrpc::writes_done(writer);

    // `writes_done` and `write` can also be coalesced by using:
    // co_await agrpc::write_last(writer, request, grpc::WriteOptions{});

    grpc::Status status;
    co_await agrpc::finish(writer, status);
    // If `status.ok()` then the server has sent its response.
    /* [full-client-streaming-client-side] */
}

asio::awaitable<void> server_streaming_rpc(example::v1::Example::Stub& stub)
{
    /* [full-server-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;

    // Returns `false` if there is a connection issue. Use `agrpc::finish` to get a `grpc::Status` with
    // error details.
    co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncServerStreaming, stub, client_context, request,
                            reader);

    example::v1::Response response;

    // Only one read may be outstanding at a time.
    // Returns `false` if the server is done sending messages or there is a connection issue. In either case
    // response should not be accessed. `agrpc::finish` can be used to get more information.
    co_await agrpc::read(reader, response);

    grpc::Status status;
    co_await agrpc::finish(reader, status);
    /* [full-server-streaming-client-side] */
}

asio::awaitable<void> bidirectional_streaming_rpc(example::v1::Example::Stub& stub)
{
    /* [full-bidirectional-client-side] */
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;

    // Returns `false` if there is a connection issue. Use `agrpc::finish` to get a `grpc::Status` with
    // error details.
    co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming, stub, client_context,
                            reader_writer);

    example::v1::Request request;

    // Only one write may be outstanding at a time. Reads can be interleaved with writes.
    // Returns `false` if the call is dead.
    co_await agrpc::write(reader_writer, request);

    // There is also an overload that takes `grpc::WriteOptions`.
    // co_await agrpc::write(writer, grpc::WriteOptions{}, request)

    // Call `writes_done` when done sending messages to the server .
    // Returns `false` if the call is dead.
    co_await agrpc::writes_done(reader_writer);

    // `writes_done` and `write` can also be coalesced by using:
    // co_await agrpc::write_last(writer, request, grpc::WriteOptions{});

    example::v1::Response response;

    // Only one read may be outstanding at a time. Reads can be interleaved with writes.
    // Returns `false` if the server is done sending messages or there is a connection issue. In either case
    // response should not be accessed. `agrpc::finish` can be used to get more information.
    co_await agrpc::read(reader_writer, response);

    grpc::Status status;
    co_await agrpc::finish(reader_writer, status);
    /* [full-bidirectional-client-side] */
}

// server
void unary_rpc(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    /* [full-unary-server-side] */
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        asio::bind_executor(grpc_context,
                            [&](grpc::ServerContext& /*server_context*/, example::v1::Request& /*request*/,
                                grpc::ServerAsyncResponseWriter<example::v1::Response>& writer) -> asio::awaitable<void>
                            {
                                example::v1::Response response;
                                co_await agrpc::finish(writer, response, grpc::Status::OK);

                                // Alternatively finish with an error.
                                co_await agrpc::finish_with_error(writer, grpc::Status::CANCELLED);
                            }));
    /* [full-unary-server-side] */
}

void client_streaming_rpc(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    /* [full-client-streaming-server-side] */
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestClientStreaming, service,
        asio::bind_executor(
            grpc_context,
            [&](grpc::ServerContext& /*server_context*/,
                grpc::ServerAsyncReader<example::v1::Response, example::v1::Request>& reader) -> asio::awaitable<void>
            {
                example::v1::Request request;

                // Only one read may be outstanding at a time.
                // Returns `false` if the client is done sending messages or there is a connection issue. In either case
                // request should not be accessed.
                co_await agrpc::read(reader, request);

                example::v1::Response response;

                // This is always ok to use even if the call is already dead.
                co_await agrpc::finish(reader, response, grpc::Status::OK);

                // Alternatively finish with an error.
                co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);
            }));
    /* [full-client-streaming-server-side] */
}

void server_streaming_rpc(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    /* [full-server-streaming-server-side] */
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestServerStreaming, service,
        asio::bind_executor(grpc_context,
                            [&](grpc::ServerContext& /*server_context*/, example::v1::Request& /*request*/,
                                grpc::ServerAsyncWriter<example::v1::Response>& writer) -> asio::awaitable<void>
                            {
                                example::v1::Response response;

                                // Only one write may be outstanding at a time.
                                // Returns `false` if the call is dead.
                                co_await agrpc::write(writer, response);

                                // `write_last` buffers the response until `finish` is called.
                                // Returns `false` if the call is dead.
                                co_await agrpc::write_last(writer, response, grpc::WriteOptions{});

                                co_await agrpc::finish(writer, grpc::Status::OK);

                                // An alternative to calling agrpc::write_last + agrpc::finish.
                                co_await agrpc::write_and_finish(writer, response, grpc::WriteOptions{},
                                                                 grpc::Status::OK);
                            }));
    /* [full-server-streaming-server-side] */
}

void bidirectional_streaming_rpc(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    /* [full-bidirectional-streaming-server-side] */
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestBidirectionalStreaming, service,
        asio::bind_executor(
            grpc_context,
            [&](grpc::ServerContext& /*server_context*/,
                grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request>& reader_writer)
                -> asio::awaitable<void>
            {
                example::v1::Request request;

                // Only one read may be outstanding at a time. Reads can be interleaved with writes.
                // Returns `false` if the client is done sending messages or there is a connection issue. In either case
                // request should not be accessed.
                co_await agrpc::read(reader_writer, request);

                example::v1::Response response;

                // Only one write may be outstanding at a time. Writes can be interleaved with reads.
                // Returns `false` if the call is dead.
                co_await agrpc::write(reader_writer, response);

                // `write_last` buffers the response until `finish` is called.
                // Returns `false` if the call is dead.
                co_await agrpc::write_last(reader_writer, response, grpc::WriteOptions{});

                co_await agrpc::finish(reader_writer, grpc::Status::OK);

                // An alternative to calling `write_last` + `finish`.
                co_await agrpc::write_and_finish(reader_writer, response, grpc::WriteOptions{}, grpc::Status::OK);
            }));
    /* [full-bidirectional-streaming-server-side] */
}
