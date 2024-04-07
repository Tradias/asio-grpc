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

#include "utils/rpc.hpp"

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"
#include "utils/client_context.hpp"
#include "utils/client_rpc.hpp"
#include "utils/grpc_format.hpp"

#include <agrpc/grpc_context.hpp>
#include <doctest/doctest.h>

namespace test
{
void client_perform_unary_success(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub,
                                  const asio::yield_context& yield, test::PerformUnarySuccessOptions options)
{
    const auto client_context = test::create_client_context();
    test::msg::Request request;
    request.set_integer(options.request_payload);
    test::msg::Response response;
    const auto status = test::UnaryClientRPC::request(grpc_context, stub, *client_context, request, response, yield);
    if (options.finish_with_error)
    {
        CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, status.error_code());
    }
    else
    {
        CHECK(status.ok());
        CHECK_EQ(21, response.integer());
    }
}

grpc::Status create_already_exists_status() { return grpc::Status{grpc::StatusCode::ALREADY_EXISTS, {}}; }
}
