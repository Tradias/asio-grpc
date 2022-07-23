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
#include "utils/grpc_generic_client_server_test.hpp"
#include "utils/protobuf.hpp"

#include <agrpc/rpc.hpp>

#include <cstddef>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcGenericClientServerTest, "yield_context generic unary")
{
    test::spawn_and_run(
        grpc_context,
        [&](asio::yield_context yield)
        {
            grpc::GenericServerContext server_context;
            grpc::GenericServerAsyncReaderWriter reader_writer{&server_context};
            CHECK(agrpc::request(service, server_context, reader_writer, yield));
            CHECK_EQ("/test.v1.Test/Unary", server_context.method());
            CHECK(agrpc::send_initial_metadata(reader_writer, yield));
            grpc::ByteBuffer buffer;
            CHECK(agrpc::read(reader_writer, buffer, yield));
            const auto request = test::grpc_buffer_to_message<test::msg::Request>(buffer);
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            const auto response_buffer = test::message_to_grpc_buffer(response);
            CHECK(agrpc::write(reader_writer, response_buffer, yield));
            CHECK(agrpc::finish(reader_writer, grpc::Status::OK, yield));
        },
        [&](asio::yield_context yield)
        {
            test::msg::Request request;
            request.set_integer(42);
            const auto request_buffer = test::message_to_grpc_buffer(request);
            const auto reader =
                agrpc::request("/test.v1.Test/Unary", *stub, client_context, request_buffer, grpc_context);
            grpc::ByteBuffer buffer;
            grpc::Status status;
            CHECK(agrpc::finish(*reader, buffer, status, yield));
            CHECK(status.ok());
            const auto response = test::grpc_buffer_to_message<test::msg::Response>(buffer);
            CHECK_EQ(21, response.integer());
        });
}

TEST_CASE_FIXTURE(test::GrpcGenericClientServerTest, "yield_context generic client streaming")
{
    bool set_initial_metadata_corked{false};
    SUBCASE("normal client_context") { set_initial_metadata_corked = false; }
    SUBCASE("client set initial metadata corked") { set_initial_metadata_corked = true; }
    test::spawn_and_run(
        grpc_context,
        [&](asio::yield_context yield)
        {
            grpc::GenericServerContext server_context;
            grpc::GenericServerAsyncReaderWriter reader_writer{&server_context};
            CHECK(agrpc::request(service, server_context, reader_writer, yield));
            CHECK_EQ("/test.v1.Test/ClientStreaming", server_context.method());
            CHECK(agrpc::send_initial_metadata(reader_writer, yield));
            grpc::ByteBuffer buffer;
            CHECK(agrpc::read(reader_writer, buffer, yield));
            const auto request = test::grpc_buffer_to_message<test::msg::Request>(buffer);
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            const auto response_buffer = test::message_to_grpc_buffer(response);
            CHECK(agrpc::write(reader_writer, response_buffer, yield));
            CHECK(agrpc::finish(reader_writer, grpc::Status::OK, yield));
        },
        [&](asio::yield_context yield)
        {
            auto reader_writer = [&]
            {
                if (set_initial_metadata_corked)
                {
                    client_context.set_initial_metadata_corked(true);
                    auto reader_writer = stub->PrepareCall(&client_context, "/test.v1.Test/ClientStreaming",
                                                           agrpc::get_completion_queue(grpc_context));
                    reader_writer->StartCall(nullptr);
                    return reader_writer;
                }
                std::unique_ptr<grpc::GenericClientAsyncReaderWriter> reader_writer;
                CHECK(agrpc::request("/test.v1.Test/ClientStreaming", *stub, client_context, reader_writer, yield));
                CHECK(agrpc::read_initial_metadata(*reader_writer, yield));
                return reader_writer;
            }();
            test::msg::Request request;
            request.set_integer(42);
            const auto request_buffer = test::message_to_grpc_buffer(request);
            CHECK(agrpc::write(*reader_writer, request_buffer, yield));
            grpc::ByteBuffer buffer;
            CHECK(agrpc::read(*reader_writer, buffer, yield));
            const auto response = test::grpc_buffer_to_message<test::msg::Response>(buffer);
            grpc::Status status;
            CHECK(agrpc::finish(*reader_writer, status, yield));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
}
}
