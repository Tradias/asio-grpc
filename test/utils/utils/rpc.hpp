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

#ifndef AGRPC_UTILS_RPC_HPP
#define AGRPC_UTILS_RPC_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"
#include "utils/client_context.hpp"
#include "utils/grpc_format.hpp"
#include "utils/time.hpp"

#include <agrpc/grpc_context.hpp>
#include <agrpc/rpc.hpp>
#include <doctest/doctest.h>

namespace test
{
template <bool IsInterface>
using ClientAsyncResponseReader =
    std::unique_ptr<std::conditional_t<IsInterface, grpc::ClientAsyncResponseReaderInterface<test::msg::Response>,
                                       grpc::ClientAsyncResponseReader<test::msg::Response>>>;

template <bool IsInterface>
using ClientAsyncReader =
    std::unique_ptr<std::conditional_t<IsInterface, grpc::ClientAsyncReaderInterface<test::msg::Response>,
                                       grpc::ClientAsyncReader<test::msg::Response>>>;

template <bool IsInterface>
using ClientAsyncWriter =
    std::unique_ptr<std::conditional_t<IsInterface, grpc::ClientAsyncWriterInterface<test::msg::Request>,
                                       grpc::ClientAsyncWriter<test::msg::Request>>>;

template <bool IsInterface>
using ClientAsyncReaderWriter = std::unique_ptr<
    std::conditional_t<IsInterface, grpc::ClientAsyncReaderWriterInterface<test::msg::Request, test::msg::Response>,
                       grpc::ClientAsyncReaderWriter<test::msg::Request, test::msg::Response>>>;

template <bool IsInterface>
using ServerAsyncWriter = std::conditional_t<IsInterface, grpc::ServerAsyncWriterInterface<test::msg::Response>&,
                                             grpc::ServerAsyncWriter<test::msg::Response>&>;

template <bool IsInterface>
using ServerAsyncReader =
    std::conditional_t<IsInterface, grpc::ServerAsyncReaderInterface<test::msg::Response, test::msg::Request>&,
                       grpc::ServerAsyncReader<test::msg::Response, test::msg::Request>&>;

template <bool IsInterface>
using ServerAsyncReaderWriter =
    std::conditional_t<IsInterface, grpc::ServerAsyncReaderWriterInterface<test::msg::Response, test::msg::Request>&,
                       grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request>&>;

struct PerformUnarySuccessOptions
{
    bool finish_with_error{false};
    int32_t request_payload{42};
};

template <class Stub>
void client_perform_unary_success(agrpc::GrpcContext& grpc_context, Stub& stub, const asio::yield_context& yield,
                                  test::PerformUnarySuccessOptions options = {})
{
    const auto client_context = test::create_client_context();
    test::msg::Request request;
    request.set_integer(options.request_payload);
    const auto reader = agrpc::request(&Stub::AsyncUnary, stub, *client_context, request, grpc_context);
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
                                    const asio::yield_context& yield,
                                    std::chrono::system_clock::time_point deadline = test::five_seconds_from_now());

struct PerformClientStreamingSuccessOptions
{
    bool finish_with_error{false};
    bool use_write_last{false};
    int32_t request_payload{42};
};

void client_perform_client_streaming_success(test::v1::Test::Stub& stub, const asio::yield_context& yield,
                                             test::PerformClientStreamingSuccessOptions options = {});

void client_perform_client_streaming_success(test::msg::Response& response,
                                             grpc::ClientAsyncWriter<test::msg::Request>& writer,
                                             const asio::yield_context& yield,
                                             test::PerformClientStreamingSuccessOptions options = {});

void client_perform_client_streaming_success(test::msg::Response& response,
                                             grpc::ClientAsyncWriterInterface<test::msg::Request>& writer,
                                             const asio::yield_context& yield,
                                             test::PerformClientStreamingSuccessOptions options = {});
}

#endif  // AGRPC_UTILS_RPC_HPP
