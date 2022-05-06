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

#ifndef AGRPC_UTILS_RPC_HPP
#define AGRPC_UTILS_RPC_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asioForward.hpp"
#include "utils/time.hpp"

#include <agrpc/grpcContext.hpp>
#include <agrpc/rpc.hpp>

namespace test
{
struct PerformUnarySuccessOptions
{
    bool finish_with_error{false};
    int32_t request_payload{42};
};

void client_perform_unary_success(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                  asio::yield_context yield, test::PerformUnarySuccessOptions options = {});

bool client_perform_unary_unchecked(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                    asio::yield_context yield,
                                    std::chrono::system_clock::time_point deadline = test::five_seconds_from_now());

struct PerformClientStreamingSuccessOptions
{
    bool finish_with_error{false};
    bool use_write_last{false};
    int32_t request_payload{42};
};

void client_perform_client_streaming_success(test::v1::Test::Stub& stub, asio::yield_context yield,
                                             test::PerformClientStreamingSuccessOptions options = {});

void client_perform_client_streaming_success(test::msg::Response& response,
                                             grpc::ClientAsyncWriter<test::msg::Request>& writer,
                                             asio::yield_context yield,
                                             test::PerformClientStreamingSuccessOptions options = {});
}

#endif  // AGRPC_UTILS_RPC_HPP
