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
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <optional>

namespace asio = boost::asio;

asio::awaitable<void> unary(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [request-unary-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        stub.AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
    /* [request-unary-client-side] */

    /* [read_initial_metadata-unary-client-side] */
    bool read_ok = co_await agrpc::read_initial_metadata(*reader, asio::use_awaitable);
    /* [read_initial_metadata-unary-client-side] */

    /* [finish-unary-client-side] */
    example::v1::Response response;
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader, response, status, asio::use_awaitable);
    /* [finish-unary-client-side] */

    silence_unused(read_ok, finish_ok);
}

asio::awaitable<void> unary_awaitable(example::v1::Example::Stub& stub)
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

    /* [write-client-streaming-client-side] */
    example::v1::Request request;
    bool write_ok = co_await agrpc::write(*writer, request, asio::use_awaitable);
    /* [write-client-streaming-client-side] */

    /* [writes_done-client-streaming-client-side] */
    bool writes_done_ok = co_await agrpc::writes_done(*writer, asio::use_awaitable);
    /* [writes_done-client-streaming-client-side] */

    /* [write_last-client-streaming-client-side] */
    bool write_last_ok = co_await agrpc::write_last(*writer, request, grpc::WriteOptions{}, asio::use_awaitable);
    /* [write_last-client-streaming-client-side] */

    /* [finish-client-streaming-client-side] */
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*writer, status, asio::use_awaitable);
    /* [finish-client-streaming-client-side] */

    silence_unused(request_ok, write_ok, writes_done_ok, write_last_ok, finish_ok);
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

    /* [read-server-streaming-client-side] */
    example::v1::Response response;
    bool read_ok = co_await agrpc::read(*reader, response, asio::use_awaitable);
    /* [read-server-streaming-client-side] */

    /* [finish-server-streaming-client-side] */
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader, status, asio::use_awaitable);
    /* [finish-server-streaming-client-side] */

    silence_unused(request_ok, read_ok, finish_ok);
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

    /* [write-bidirectional-client-side] */
    example::v1::Request request;
    bool write_ok = co_await agrpc::write(*reader_writer, request, asio::use_awaitable);
    /* [write-bidirectional-client-side] */

    /* [write_done-bidirectional-client-side] */
    bool writes_done_ok = co_await agrpc::writes_done(*reader_writer, asio::use_awaitable);
    /* [write_done-bidirectional-client-side] */

    /* [write_last-bidirectional-client-side] */
    bool write_last_ok = co_await agrpc::write_last(*reader_writer, request, grpc::WriteOptions{}, asio::use_awaitable);
    /* [write_last-bidirectional-client-side] */

    /* [read-bidirectional-client-side] */
    example::v1::Response response;
    bool read_ok = co_await agrpc::read(*reader_writer, response, asio::use_awaitable);
    /* [read-bidirectional-client-side] */

    /* [finish-bidirectional-client-side] */
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(*reader_writer, status, asio::use_awaitable);
    /* [finish-bidirectional-client-side] */

    silence_unused(request_ok, write_ok, writes_done_ok, write_last_ok, read_ok, finish_ok);
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

asio::awaitable<void> client_generic_streaming(grpc::GenericStub& stub)
{
    /* [request-generic-streaming-client-side] */
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> reader_writer;
    bool request_ok = co_await agrpc::request("/example.v1.Example/BidirectionalStreaming", stub, client_context,
                                              reader_writer, asio::use_awaitable);
    /* [request-generic-streaming-client-side] */
}

void client_generic_streaming_corked(grpc::GenericStub& stub, agrpc::GrpcContext& grpc_context)
{
    /* [request-client-generic-streaming-corked] */
    grpc::ClientContext client_context;
    client_context.set_initial_metadata_corked(true);
    auto reader_writer = stub.PrepareCall(&client_context, "/example.v1.Example/BidirectionalStreaming",
                                          agrpc::get_completion_queue(grpc_context));
    reader_writer->StartCall(nullptr);
    /* [request-client-generic-streaming-corked] */
}

void create_grpc_context()
{
    /* [create-grpc_context-client-side] */
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    /* [create-grpc_context-client-side] */
}

asio::awaitable<void> bind_allocator(std::allocator<std::byte> my_allocator)
{
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;

    /* [bind_allocator-client-side] */
    co_await agrpc::writes_done(*writer, agrpc::bind_allocator(my_allocator, asio::use_awaitable));
    /* [bind_allocator-client-side] */
}

asio::awaitable<void> async_notify_on_state_change(const std::string& host)
{
    /* [grpc_initiate-NotifyOnStateChange] */
    auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    auto state = channel->GetState(true);
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    bool is_deadline_not_expired = co_await agrpc::grpc_initiate(
        [&](agrpc::GrpcContext& grpc_context, void* tag)
        {
            channel->NotifyOnStateChange(state, deadline, agrpc::get_completion_queue(grpc_context), tag);
        },
        asio::use_awaitable);
    /* [grpc_initiate-NotifyOnStateChange] */

    silence_unused(is_deadline_not_expired);
}

void poll_context(agrpc::GrpcContext& grpc_context)
{
    /* [poll_context-with-io_context] */
    asio::io_context io_context;
    agrpc::PollContext poll_context{io_context.get_executor()};
    std::optional guard{asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)};
    poll_context.async_poll(grpc_context);

    // Use io_context and grpc_context and reset the guard when done.

    io_context.run();
    /* [poll_context-with-io_context] */
}

auto hundred_milliseconds_from_now() { return std::chrono::system_clock::now() + std::chrono::milliseconds(100); }

asio::awaitable<void> server_streaming_cancel_safe(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [cancel-safe-server-streaming] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    co_await agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, reader);

    agrpc::GrpcCancelSafe safe;  // equivalent to agrpc::CancelSafe<bool>

    // Initiate a read with cancellation safety.
    example::v1::Response response;
    agrpc::read(*reader, response, asio::bind_executor(grpc_context, safe.token()));

    grpc::Alarm alarm;
    bool ok{true};
    while (ok)
    {
        using namespace asio::experimental::awaitable_operators;
        auto variant = co_await (agrpc::wait(alarm, hundred_milliseconds_from_now()) || safe.wait(asio::use_awaitable));
        if (0 == variant.index())  // Alarm finished
        {
            // The read continues in the background.
        }
        else  // Read finished
        {
            ok = std::get<1>(variant);
            if (ok)
            {
                // Initiate the next read.
                agrpc::read(*reader, response, asio::bind_executor(grpc_context, safe.token()));
            }
        }
    }
    /* [cancel-safe-server-streaming] */
}

int main()
{
    auto stub =
        example::v1::Example::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    // begin-snippet: create-grpc_context-client-side
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    // end-snippet

    // begin-snippet: make-work-guard
    std::optional guard{asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)};
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