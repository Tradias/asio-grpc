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

#ifndef AGRPC_UTILS_EXECUTION_TEST_HPP
#define AGRPC_UTILS_EXECUTION_TEST_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/client_rpc.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/execution_utils.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/server_rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/asio_grpc.hpp>

namespace test
{
template <class Base>
struct ExecutionTestMixin : Base
{
    template <class... Sender>
    void run(Sender&&... sender)
    {
        this->grpc_context.work_started();
        stdexec::sync_wait(stdexec::when_all(
            exec::finally(test::with_inline_scheduler(stdexec::when_all(std::forward<Sender>(sender)...)),
                          stdexec::then(stdexec::just(),
                                        [&]
                                        {
                                            this->grpc_context.work_finished();
                                        })),
            stdexec::then(stdexec::just(),
                          [&]
                          {
                              this->grpc_context.run();
                          })));
    }
};

struct ExecutionGrpcContextTest : test::ExecutionTestMixin<test::GrpcContextTest>
{
};

struct ExecutionRpcHandlerTest : test::ExecutionTestMixin<test::GrpcClientServerTest>
{
    struct Context
    {
        explicit Context(std::chrono::system_clock::time_point deadline)
        {
            context.set_deadline(deadline);
            request.set_integer(42);
        }

        grpc::ClientContext context;
        test::msg::Request request;
        test::msg::Response response;
    };

    template <class OnRequestDone>
    auto make_client_unary_request_sender(std::chrono::system_clock::time_point deadline,
                                          OnRequestDone on_request_done = test::NoOp{})
    {
        return stdexec::then(stdexec::just(),
                             [deadline]()
                             {
                                 return std::make_unique<Context>(deadline);
                             }) |
               stdexec::let_value(
                   [this, on_request_done](auto& context)
                   {
                       auto& [client_context, request, response] = *context;
                       return stdexec::then(test::unstoppable(test::UnaryClientRPC::request(
                                                grpc_context, *stub, client_context, request, response)),
                                            [&context, on_request_done](const grpc::Status& status)
                                            {
                                                auto& [client_context, request, response] = *context;
                                                on_request_done(response, status);
                                            });
                   });
    }

    static void check_response_ok(const test::msg::Response& response, const grpc::Status& status)
    {
        CHECK_EQ(grpc::StatusCode::OK, status.error_code());
        CHECK_EQ(24, response.integer());
    }

    static void check_status_not_ok(const test::msg::Response&, const grpc::Status& status)
    {
        CHECK_FALSE(status.ok());
    }

    auto make_client_unary_request_sender(int& request_count, int max_request_count)
    {
        return make_client_unary_request_sender(
            test::five_seconds_from_now(),
            [&, max_request_count](const test::msg::Response& response, const grpc::Status& status)
            {
                check_response_ok(response, status);
                ++request_count;
                if (request_count == max_request_count)
                {
                    shutdown.initiate();
                }
            });
    }

    auto handle_unary_request_sender(test::UnaryServerRPC& rpc, test::msg::Request& request)
    {
        CHECK_EQ(42, request.integer());
        return stdexec::let_value(stdexec::just(test::msg::Response{}),
                                  [&](auto& response)
                                  {
                                      response.set_integer(24);
                                      return rpc.finish(response, grpc::Status::OK);
                                  });
    }

    auto make_unary_rpc_handler_sender()
    {
        using ServerRPC = test::UnaryServerRPC;
        return test::with_query_value(
            agrpc::register_sender_rpc_handler<ServerRPC>(grpc_context, service,
                                                          [&](ServerRPC& rpc, test::msg::Request& request)
                                                          {
                                                              return handle_unary_request_sender(rpc, request);
                                                          }),
            stdexec::get_allocator, get_allocator());
    }

    test::ServerShutdownInitiator shutdown{*server};
};

template <class RPC>
struct ExecutionClientRPCTest : test::ExecutionTestMixin<test::ClientServerRPCTest<RPC>>
{
    using Base = test::ClientServerRPCTest<RPC>;

#if !UNIFEX_NO_COROUTINES
    template <class RPCHandler, class... ClientFunctions>
    void register_and_perform_requests(RPCHandler&& handler, ClientFunctions&&... client_functions)
    {
        int counter{};
        this->run(
            agrpc::register_sender_rpc_handler<typename Base::ServerRPC>(this->grpc_context, this->service, handler),
            [&counter, &client_functions, &server_shutdown = this->server_shutdown]() -> exec::task<void>
            {
                typename Base::ClientRPC::Request request;
                typename Base::ClientRPC::Response response;
                co_await client_functions(request, response);
                ++counter;
                if (counter == sizeof...(client_functions))
                {
                    server_shutdown.initiate();
                }
            }()...);
    }
#endif
};
}

#endif  // AGRPC_UTILS_EXECUTION_TEST_HPP
