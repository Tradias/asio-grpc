// Copyright 2024 Dennis Hezel
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
#include "helloworld/helloworld.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <cassert>
#include <optional>

namespace asio = boost::asio;

static_assert(asio::is_executor<agrpc::GrpcExecutor>::value);

asio::awaitable<void> agrpc_notify_on_state_change(agrpc::GrpcContext& grpc_context, const std::string& host)
{
    /* [notify_on_state_change] */
    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto state = channel->GetState(true);
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    bool has_state_changed =
        co_await agrpc::notify_on_state_change(grpc_context, *channel, state, deadline, asio::use_awaitable);
    /* [notify_on_state_change] */

    silence_unused(has_state_changed);
}

auto hundred_milliseconds_from_now() { return std::chrono::system_clock::now() + std::chrono::milliseconds(100); }

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
    using RPC = agrpc::ClientRPC<&example::v1::Example::StubInterface::AsyncUnary>;
    grpc::ClientContext client_context;
    example::v1::Response response;
    example::v1::Request request;
    const grpc::Status status =
        co_await RPC::request(grpc_context, mock_stub, client_context, request, response, asio::use_awaitable);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(42, response.integer());
    /* [mock-stub] */
}

void client_main()
{
    // begin-snippet: client-side-hello-world
    helloworld::Greeter::Stub stub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context;
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            using RPC = agrpc::ClientRPC<&helloworld::Greeter::Stub::PrepareAsyncSayHello>;
            grpc::ClientContext client_context;
            helloworld::HelloRequest request;
            request.set_name("world");
            helloworld::HelloReply response;
            const grpc::Status status =
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
