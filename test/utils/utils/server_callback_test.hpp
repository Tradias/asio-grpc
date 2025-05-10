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

#ifndef AGRPC_UTILS_SERVER_RPC_HPP
#define AGRPC_UTILS_SERVER_RPC_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"
#include "utils/client_context.hpp"
#include "utils/grpc_client_server_callback_test.hpp"
#include "utils/io_context_test.hpp"

#include <agrpc/client_callback.hpp>
#include <doctest/doctest.h>

#include <future>

namespace test
{
struct ServerCallbackTest : test::GrpcClientServerCallbackTest, test::IoContextTest
{
    using Request = test::msg::Request;
    using Response = test::msg::Response;

    ServerCallbackTest()
    {
        run_io_context_detached();
        test::set_default_deadline(client_context);
    }

    auto make_unary_request()
    {
        Request request;
        Response response;
        auto status = agrpc::request(&test::v1::Test::Stub::async::Unary, stub->async(), client_context, request,
                                     response, asio::use_future)
                          .get();
        return std::pair{std::move(status), std::move(response)};
    }

    void wait_for_server_done() { server_done_promise.get_future().wait(); }

    void server_done() { server_done_promise.set_value(); }

    std::promise<void> server_done_promise;
};
}

#endif  // AGRPC_UTILS_SERVER_RPC_HPP
