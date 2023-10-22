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

#include "example/v1/example_mock.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <cassert>
#include <optional>

namespace asio = boost::asio;

static_assert(asio::is_executor<agrpc::GrpcExecutor>::value);

asio::awaitable<void> unary(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [request-unary-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context, request, grpc_context);
    /* [request-unary-client-side] */

    /* [read_initial_metadata-unary-client-side] */
    bool read_ok = co_await agrpc::read_initial_metadata(reader, asio::use_awaitable);
    /* [read_initial_metadata-unary-client-side] */

    /* [finish-unary-client-side] */
    example::v1::Response response;
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(reader, response, status, asio::use_awaitable);
    /* [finish-unary-client-side] */

    silence_unused(read_ok, finish_ok);
}

asio::awaitable<void> client_streaming(example::v1::Example::Stub& stub)
{
    /* [request-client-streaming-client-side] */
    grpc::ClientContext client_context;
    example::v1::Response response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncClientStreaming, stub,
                                              client_context, writer, response, asio::use_awaitable);
    /* [request-client-streaming-client-side] */

    /* [write-client-streaming-client-side] */
    example::v1::Request request;
    bool write_ok = co_await agrpc::write(writer, request, asio::use_awaitable);
    /* [write-client-streaming-client-side] */

    /* [writes_done-client-streaming-client-side] */
    bool writes_done_ok = co_await agrpc::writes_done(writer, asio::use_awaitable);
    /* [writes_done-client-streaming-client-side] */

    /* [write_last-client-streaming-client-side] */
    bool write_last_ok = co_await agrpc::write_last(writer, request, grpc::WriteOptions{}, asio::use_awaitable);
    /* [write_last-client-streaming-client-side] */

    /* [finish-client-streaming-client-side] */
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(writer, status, asio::use_awaitable);
    /* [finish-client-streaming-client-side] */

    silence_unused(request_ok, write_ok, writes_done_ok, write_last_ok, finish_ok);
}

asio::awaitable<void> client_streaming_alt(example::v1::Example::Stub& stub)
{
    /* [request-client-streaming-client-side-alt] */
    grpc::ClientContext client_context;
    example::v1::Response response;
    auto [writer, request_ok] = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncClientStreaming, stub,
                                                        client_context, response, asio::use_awaitable);
    /* [request-client-streaming-client-side-alt] */

    silence_unused(writer, request_ok);
}

void client_streaming_corked(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
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
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncServerStreaming, stub,
                                              client_context, request, reader, asio::use_awaitable);
    /* [request-server-streaming-client-side] */

    /* [read-server-streaming-client-side] */
    example::v1::Response response;
    bool read_ok = co_await agrpc::read(reader, response, asio::use_awaitable);
    /* [read-server-streaming-client-side] */

    /* [finish-server-streaming-client-side] */
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(reader, status, asio::use_awaitable);
    /* [finish-server-streaming-client-side] */

    silence_unused(request_ok, read_ok, finish_ok);
}

asio::awaitable<void> server_streaming_alt(example::v1::Example::Stub& stub)
{
    /* [request-server-streaming-client-side-alt] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    auto [reader, request_ok] = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncServerStreaming, stub,
                                                        client_context, request, asio::use_awaitable);
    /* [request-server-streaming-client-side-alt] */

    silence_unused(reader, request_ok);
}

asio::awaitable<void> bidirectional_streaming(example::v1::Example::Stub& stub)
{
    /* [request-bidirectional-client-side] */
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;
    bool request_ok = co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming, stub,
                                              client_context, reader_writer, asio::use_awaitable);
    /* [request-bidirectional-client-side] */

    /* [write-bidirectional-client-side] */
    example::v1::Request request;
    bool write_ok = co_await agrpc::write(reader_writer, request, asio::use_awaitable);
    /* [write-bidirectional-client-side] */

    /* [write_done-bidirectional-client-side] */
    bool writes_done_ok = co_await agrpc::writes_done(reader_writer, asio::use_awaitable);
    /* [write_done-bidirectional-client-side] */

    /* [write_last-bidirectional-client-side] */
    bool write_last_ok = co_await agrpc::write_last(reader_writer, request, grpc::WriteOptions{}, asio::use_awaitable);
    /* [write_last-bidirectional-client-side] */

    /* [read-bidirectional-client-side] */
    example::v1::Response response;
    bool read_ok = co_await agrpc::read(reader_writer, response, asio::use_awaitable);
    /* [read-bidirectional-client-side] */

    /* [finish-bidirectional-client-side] */
    grpc::Status status;
    bool finish_ok = co_await agrpc::finish(reader_writer, status, asio::use_awaitable);
    /* [finish-bidirectional-client-side] */

    silence_unused(request_ok, write_ok, writes_done_ok, write_last_ok, read_ok, finish_ok);
}

asio::awaitable<void> bidirectional_streaming_alt(example::v1::Example::Stub& stub)
{
    /* [request-bidirectional-client-side-alt] */
    grpc::ClientContext client_context;
    auto [reader_writer, request_ok] = co_await agrpc::request(
        &example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming, stub, client_context, asio::use_awaitable);
    /* [request-bidirectional-client-side-alt] */

    silence_unused(reader_writer, request_ok);
}

void bidirectional_streaming_corked(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [request-client-bidirectional-client-side-corked] */
    grpc::ClientContext client_context;
    client_context.set_initial_metadata_corked(true);
    auto reader_writer =
        stub.AsyncBidirectionalStreaming(&client_context, agrpc::get_completion_queue(grpc_context), nullptr);
    /* [request-client-bidirectional-client-side-corked] */
}

void client_generic_unary_request(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub)
{
    /* [request-generic-unary-client-side] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    grpc::ByteBuffer buffer;
    bool own_buffer;
    grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Request>(request, &buffer, &own_buffer);
    (void)own_buffer;
    std::unique_ptr<grpc::GenericClientAsyncResponseReader> reader =
        agrpc::request("/example.v1.Example/Unary", stub, client_context, buffer, grpc_context);
    /* [request-generic-unary-client-side] */
}

asio::awaitable<void> client_generic_streaming_request(grpc::GenericStub& stub)
{
    /* [request-generic-streaming-client-side] */
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> reader_writer;
    bool request_ok = co_await agrpc::request("/example.v1.Example/BidirectionalStreaming", stub, client_context,
                                              reader_writer, asio::use_awaitable);
    /* [request-generic-streaming-client-side] */

    silence_unused(request_ok);
}

void client_generic_streaming_corked(agrpc::GrpcContext& grpc_context, grpc::GenericStub& stub)
{
    /* [request-client-generic-streaming-corked] */
    grpc::ClientContext client_context;
    client_context.set_initial_metadata_corked(true);
    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> reader_writer = stub.PrepareCall(
        &client_context, "/example.v1.Example/BidirectionalStreaming", agrpc::get_completion_queue(grpc_context));
    reader_writer->StartCall(nullptr);
    /* [request-client-generic-streaming-corked] */
}

asio::awaitable<void> bind_allocator(std::allocator<void> my_allocator)
{
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;

    /* [bind_allocator-client-side] */
    co_await agrpc::writes_done(writer, agrpc::bind_allocator(my_allocator, asio::use_awaitable));
    /* [bind_allocator-client-side] */
}

asio::awaitable<void> grpc_initate_notify_on_state_change(const std::string& host)
{
    /* [grpc_initiate-NotifyOnStateChange] */
    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto state = channel->GetState(true);
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    bool has_state_changed = co_await agrpc::grpc_initiate(
        [&](agrpc::GrpcContext& grpc_context, void* tag)
        {
            channel->NotifyOnStateChange(state, deadline, agrpc::get_completion_queue(grpc_context), tag);
        },
        asio::use_awaitable);
    /* [grpc_initiate-NotifyOnStateChange] */

    silence_unused(has_state_changed);
}

asio::awaitable<void> agrpc_notify_on_state_change(agrpc::GrpcContext& grpc_context, const std::string& host)
{
    /* [notify_on_state_change] */
    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto state = channel->GetState(true);
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    bool has_state_changed = co_await agrpc::notify_on_state_change(grpc_context, *channel, state, deadline);
    /* [notify_on_state_change] */

    silence_unused(has_state_changed);
}

auto hundred_milliseconds_from_now() { return std::chrono::system_clock::now() + std::chrono::milliseconds(100); }

asio::awaitable<void> server_streaming_cancel_safe(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    /* [cancel-safe-server-streaming] */
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncServerStreaming, stub, client_context, request,
                            reader);

    agrpc::GrpcCancelSafe safe;  // equivalent to agrpc::CancelSafe<void(bool)>

    // Initiate a read with cancellation safety.
    example::v1::Response response;
    agrpc::read(reader, response, asio::bind_executor(grpc_context, safe.token()));

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
                agrpc::read(reader, response, asio::bind_executor(grpc_context, safe.token()));
            }
        }
    }
    /* [cancel-safe-server-streaming] */
}

asio::awaitable<void> mock_stub(agrpc::GrpcContext& grpc_context)
{
    /* [mock-stub] */
    // Setup mock stub
    struct MockResponseReader : grpc::ClientAsyncResponseReaderInterface<example::v1::Response>
    {
        MOCK_METHOD0(StartCall, void());
        MOCK_METHOD1(ReadInitialMetadata, void(void*));
        MOCK_METHOD3(Finish, void(example::v1::Response*, grpc::Status*, void*));
    };
    testing::NiceMock<example::v1::MockExampleStub> mock_stub;
    testing::NiceMock<MockResponseReader> mock_reader;
    EXPECT_CALL(mock_reader, Finish)
        .WillOnce(
            [&](example::v1::Response* response, grpc::Status* status, void* tag)
            {
                *status = grpc::Status::OK;
                response->set_integer(42);
                agrpc::process_grpc_tag(grpc_context, tag, true);
            });
    EXPECT_CALL(mock_stub, AsyncUnaryRaw).WillOnce(testing::Return(&mock_reader));

    // Inject mock_stub into code under test
    grpc::ClientContext client_context;
    example::v1::Request request;
    const auto writer = agrpc::request(&example::v1::Example::StubInterface::AsyncUnary, mock_stub, client_context,
                                       request, grpc_context);
    grpc::Status status;
    example::v1::Response response;
    co_await agrpc::finish(writer, response, status);

    assert(status.ok());
    assert(42 == response.integer());
    /* [mock-stub] */
}

void client_main()
{
    // begin-snippet: create-grpc_context-client-side
    agrpc::GrpcContext grpc_context;
    // end-snippet

    // begin-snippet: run-grpc_context-client-side
    example::v1::Example::Stub stub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            grpc::ClientContext client_context;
            example::v1::Request request;
            request.set_integer(42);
            example::v1::Response response;
            using RPC = agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncUnary>;
            grpc::Status status =
                co_await RPC::request(grpc_context, stub, client_context, request, response, asio::use_awaitable);
            assert(status.ok());
        },
        asio::detached);
    grpc_context.run();
    // end-snippet
}

void client_main_cheat_sheet()
{
    /* [client-main-cheat-sheet] */
    agrpc::GrpcContext grpc_context;
    example::v1::Example::Stub stub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            // ...
            co_return;
        },
        asio::detached);
    grpc_context.run();
    /* [client-main-cheat-sheet] */
}
