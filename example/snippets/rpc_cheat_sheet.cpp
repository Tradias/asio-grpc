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

#include "example/v1/example_mock.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/rpc.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

asio::awaitable<void> unary(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [full-unary-client-side] */
    grpc::ClientContext client_context;

    // Always set a deadline.
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context, request, grpc_context);

    // If `read_ok` is `false` then the call is dead. Use `agrpc::finish` to get a `grpc::Status` with more
    // error details.
    bool read_ok = co_await agrpc::read_initial_metadata(reader);

    example::v1::Response response;
    grpc::Status status;
    co_await agrpc::finish(reader, response, status);
    // If `status.ok()` then the server has sent its response.
    /* [full-unary-client-side] */

    silence_unused(read_ok);
}

asio::awaitable<void> client_streaming(example::v1::Example::Stub& stub)
{
    /* [full-client-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Response response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;

    // If `request_ok` is `false` then there is a connection issue. Use `agrpc::finish` to get a `grpc::Status` with
    // error details.
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncClientStreaming, stub,
                                              client_context, writer, response);

    example::v1::Request request;

    // Only one write may be outstanding at a time.
    // If `write_ok` is `false` the the call is dead.
    bool write_ok = co_await agrpc::write(writer, request);

    // There is also an overload that takes `grpc::WriteOptions`.
    // co_await agrpc::write(writer, grpc::WriteOptions{}, request)

    // Call `writes_done` when done sending messages to the server .
    // If `writes_done_ok` is `false` the the call is dead.
    bool writes_done_ok = co_await agrpc::writes_done(writer);

    // `writes_done` and `write` can also be coalesced by using:
    // bool writes_done_ok = co_await agrpc::write_last(writer, request, grpc::WriteOptions{});

    grpc::Status status;
    co_await agrpc::finish(writer, status);
    // If `status.ok()` then the server has sent its response.
    /* [full-client-streaming-client-side] */

    silence_unused(request_ok, write_ok, writes_done_ok);
}

asio::awaitable<void> server_streaming(example::v1::Example::Stub& stub)
{
    /* [full-server-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;

    // If `request_ok` is `false` then there is a connection issue. Use `agrpc::finish` to get a `grpc::Status` with
    // error details.
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncServerStreaming, stub,
                                              client_context, request, reader);

    example::v1::Response response;

    // Only one read may be outstanding at a time.
    // If `read_ok` is `false` the the server is done sending messages or there is a connection issue. In either case
    // response should not be accessed. `agrpc::finish` can be used to get more information.
    bool read_ok = co_await agrpc::read(reader, response);

    grpc::Status status;
    co_await agrpc::finish(reader, status);
    /* [full-server-streaming-client-side] */

    silence_unused(request_ok, read_ok);
}

asio::awaitable<void> bidirectional_streaming(example::v1::Example::Stub& stub)
{
    /* [full-bidirectional-client-side] */
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;

    // If `request_ok` is `false` then there is a connection issue. Use `agrpc::finish` to get a `grpc::Status` with
    // error details.
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming, stub,
                                              client_context, reader_writer);

    example::v1::Request request;

    // Only one write may be outstanding at a time. Reads can be interleaved with writes.
    // If `write_ok` is `false` the the call is dead.
    bool write_ok = co_await agrpc::write(reader_writer, request);

    // There is also an overload that takes `grpc::WriteOptions`.
    // co_await agrpc::write(writer, grpc::WriteOptions{}, request)

    // Call `writes_done` when done sending messages to the server .
    // If `writes_done_ok` is `false` the the call is dead.
    bool writes_done_ok = co_await agrpc::writes_done(reader_writer);

    // `writes_done` and `write` can also be coalesced by using:
    // bool writes_done_ok = co_await agrpc::write_last(writer, request, grpc::WriteOptions{});

    example::v1::Response response;

    // Only one read may be outstanding at a time. Reads can be interleaved with writes.
    // If `read_ok` is `false` the the server is done sending messages or there is a connection issue. In either case
    // response should not be accessed. `agrpc::finish` can be used to get more information.
    bool read_ok = co_await agrpc::read(reader_writer, response);

    grpc::Status status;
    co_await agrpc::finish(reader_writer, status);
    /* [full-bidirectional-client-side] */

    silence_unused(request_ok, write_ok, writes_done_ok, read_ok);
}
