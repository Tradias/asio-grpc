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

#include "utils/rpc.hpp"

#include "test/v1/test.grpc.pb.h"
#include "utils/asioForward.hpp"
#include "utils/clientContext.hpp"
#include "utils/time.hpp"

#include <agrpc/grpcContext.hpp>
#include <agrpc/rpc.hpp>
#include <doctest/doctest.h>

namespace test
{
void client_perform_unary_success(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                  asio::yield_context yield, test::PerformUnarySuccessOptions options)
{
    auto client_context = create_client_context();
    test::msg::Request request;
    request.set_integer(options.request_payload);
    auto reader = stub.AsyncUnary(client_context.get(), request, agrpc::get_completion_queue(grpc_context));
    test::msg::Response response;
    grpc::Status status;
    CHECK(agrpc::finish(*reader, response, status, yield));
    if (options.finish_with_error)
    {
        CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    }
    else
    {
        CHECK(status.ok());
        CHECK_EQ(21, response.integer());
    }
}

bool client_perform_unary_unchecked(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                    asio::yield_context yield, std::chrono::system_clock::time_point deadline)
{
    auto client_context = create_client_context(deadline);
    auto reader =
        stub.AsyncUnary(client_context.get(), test::msg::Request{}, agrpc::get_completion_queue(grpc_context));
    test::msg::Response response;
    grpc::Status status;
    return agrpc::finish(*reader, response, status, yield);
}

void client_perform_client_streaming_success(test::v1::Test::Stub& stub, asio::yield_context yield,
                                             test::PerformClientStreamingSuccessOptions options)
{
    test::msg::Response response;
    auto client_context = create_client_context();
    auto [writer, ok] =
        agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, stub, *client_context, response, yield);
    CHECK(ok);
    test::client_perform_client_streaming_success(response, *writer, yield, options);
}

void client_perform_client_streaming_success(test::msg::Response& response,
                                             grpc::ClientAsyncWriter<test::msg::Request>& writer,
                                             asio::yield_context yield,
                                             test::PerformClientStreamingSuccessOptions options)
{
    CHECK(agrpc::read_initial_metadata(writer, yield));
    test::msg::Request request;
    request.set_integer(options.request_payload);
    CHECK(agrpc::write(writer, request, yield));
    if (options.use_write_last)
    {
        CHECK(agrpc::write_last(writer, request, grpc::WriteOptions{}, yield));
    }
    else
    {
        CHECK(agrpc::write(writer, request, grpc::WriteOptions{}, yield));
        CHECK(agrpc::writes_done(writer, yield));
    }
    grpc::Status status;
    CHECK(agrpc::finish(writer, status, yield));
    if (options.finish_with_error)
    {
        CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    }
    else
    {
        CHECK(status.ok());
        CHECK_EQ(21, response.integer());
    }
}
}
