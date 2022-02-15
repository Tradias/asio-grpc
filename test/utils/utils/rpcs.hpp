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

#ifndef AGRPC_UTILS_RPCS_HPP
#define AGRPC_UTILS_RPCS_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asioForward.hpp"
#include "utils/time.hpp"

#include <agrpc/grpcContext.hpp>
#include <agrpc/rpcs.hpp>

namespace test
{
struct PerformOptions
{
    bool finish_with_error{false};
};

void client_perform_unary_success(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                  asio::yield_context yield, test::PerformOptions options = {});

void client_perform_unary_unchecked(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                    asio::yield_context yield);

void client_perform_client_streaming_success(test::v1::Test::Stub& stub, asio::yield_context yield,
                                             test::PerformOptions options = {});

void client_perform_client_streaming_success(test::msg::Response& response,
                                             grpc::ClientAsyncWriter<test::msg::Request>& writer,
                                             asio::yield_context yield, test::PerformOptions options = {});
}

#endif  // AGRPC_UTILS_RPCS_HPP
