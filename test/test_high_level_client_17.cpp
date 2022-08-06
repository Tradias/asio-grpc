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

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/rpc.hpp"

#include <agrpc/high_level_client.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <auto RPC, class Executor>
bool a(agrpc::BasicRpc<RPC, Executor>& c)
{
    return c.ok();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "rpc")
{
    using Stub = test::v1::Test::Stub;
    using StubInterface = test::v1::Test::StubInterface;
    // constexpr bool IS_STUB_INTERFACE = false;
    // test::GrpcClientServerTest test;
    // Stub& test_stub = *test.stub;
    // agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    // Stub stub{grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials())};

    // bool use_write_and_finish{false};
    // SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    // bool use_write_last{false};
    // SUBCASE("server write_last") { use_write_last = true; }
    // bool use_client_convenience{false};
    // SUBCASE("client use convenience") { use_client_convenience = true; }
    test::spawn_and_run(
        grpc_context,
        [&](asio::yield_context yield)
        {
            test::msg::Request request;
            grpc::ServerAsyncWriter<test::msg::Response> writer{&server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, service, server_context,
                                 request, writer, yield));
            test::ServerAsyncWriter<false> writer_ref = writer;
            CHECK(agrpc::send_initial_metadata(writer_ref, yield));
            // CHECK_EQ(42, request.integer());
            // test::msg::Response response;
            // response.set_integer(21);
            // CHECK(agrpc::write(writer_ref, response, grpc::WriteOptions{}, yield));
            // if (use_write_and_finish)
            // {
            //     CHECK(agrpc::write_and_finish(writer_ref, response, {}, grpc::Status::OK, yield));
            // }
            // else
            // {
            //     if (use_write_last)
            //     {
            //         CHECK(agrpc::write_last(writer_ref, response, grpc::WriteOptions{}, yield));
            //     }
            //     else
            //     {
            //         CHECK(agrpc::write(writer_ref, response, yield));
            //     }
            CHECK(agrpc::finish(writer_ref, grpc::Status::OK, yield));
            // }
        },
        [&](asio::yield_context yield)
        {
            test::msg::Request request;
            request.set_integer(42);
            using Rpc = agrpc::Rpc<&Stub::PrepareAsyncServerStreaming>;
            auto call = Rpc::start(grpc_context, *stub, client_context, request, yield);
            CHECK(a(call));
            agrpc::request(&Stub::PrepareAsyncServerStreaming, stub, client_context, request, call.responder, yield);
            // test::msg::Response response;

            // CHECK(ok);
            CHECK(call.read_initial_metadata(yield));
            // agrpc::Rpc<&Stub::PrepareAsyncBidirectionalStreaming>::start(grpc_context, *stub, client_context, yield);
            // agrpc::Rpc<&Stub::PrepareAsyncUnary>::start(grpc_context, *stub, client_context, request, yield);
            //  test::msg::Response response;
            //  CHECK(agrpc::read(*reader, response, yield));
            //  CHECK(agrpc::read(reader, response, yield));
            //  grpc::Status status;
            //  CHECK(agrpc::finish(*reader, status, yield));
            //  CHECK(status.ok());
            //  CHECK_EQ(21, response.integer());
        });
}

#endif