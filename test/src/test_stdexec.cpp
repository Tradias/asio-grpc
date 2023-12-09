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

#include "utils/client_rpc_test.hpp"
#include "utils/server_rpc.hpp"
#include "utils/time.hpp"

#include <agrpc/asio_grpc.hpp>
#include <exec/finally.hpp>
#include <exec/task.hpp>
#include <stdexec/execution.hpp>

template <class ClientRPC>
struct StdexecTest : test::ClientServerRPCTest<ClientRPC>
{
    template <class... Sender>
    void run(Sender&&... sender)
    {
        this->grpc_context.work_started();
        stdexec::sync_wait(stdexec::when_all(exec::finally(stdexec::when_all(std::forward<Sender>(sender)...),
                                                           stdexec::then(stdexec::just(),
                                                                         [&]()
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

TEST_CASE_FIXTURE(StdexecTest<test::UnaryClientRPC>, "stdexec UnaryClientRPC success")
{
    run(agrpc::register_sender_rpc_handler<ServerRPC>(grpc_context, service,
                                                      [&](ServerRPC& rpc, Request& request)
                                                      {
                                                          CHECK_EQ(1, request.integer());
                                                          return stdexec::let_value(stdexec::just(Response{}),
                                                                                    [&](Response& response)
                                                                                    {
                                                                                        response.set_integer(11);
                                                                                        return rpc.finish(
                                                                                            response, grpc::Status::OK);
                                                                                    });
                                                      }),
        stdexec::just(Request{}, Response{}) |
            stdexec::let_value(
                [&](Request& request, Response& response)
                {
                    request.set_integer(1);
                    return request_rpc(client_context, request, response, agrpc::use_sender);
                }) |
            stdexec::then(
                [&](const grpc::Status& status)
                {
                    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
                    server_shutdown.initiate();
                }));
}

TEST_CASE_FIXTURE(StdexecTest<test::UnaryClientRPC>,
                  "stdexec Unary ClientRPC::request automatically finishes rpc on error")
{
    server->Shutdown();
    client_context.set_deadline(test::ten_milliseconds_from_now());
    ClientRPC::Request request;
    ClientRPC::Response response;
    run(stdexec::then(request_rpc(true, client_context, request, response, agrpc::use_sender),
                      [](const auto& status)
                      {
                          const auto status_code = status.error_code();
                          CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == status_code ||
                                         grpc::StatusCode::UNAVAILABLE == status_code),
                                        status_code);
                      }));
}

TEST_CASE_FIXTURE(StdexecTest<test::ClientStreamingClientRPC>, "stdexec ClientStreamingRPC wait_for_done")
{
    bool is_cancelled{true};
    ClientRPC rpc{grpc_context};
    Response response;
    run(agrpc::register_sender_rpc_handler<test::NotifyWhenDoneClientStreamingServerRPC>(
            grpc_context, service,
            [&](test::NotifyWhenDoneClientStreamingServerRPC& rpc)
            {
                return stdexec::when_all(stdexec::then(rpc.wait_for_done(),
                                                       [&]()
                                                       {
                                                           is_cancelled = rpc.context().IsCancelled();
                                                       }),
                                         stdexec::let_value(stdexec::just(Response{}),
                                                            [&](Response& response)
                                                            {
                                                                return rpc.finish(response, grpc::Status::OK);
                                                            }));
            }),
        stdexec::just(Request{}) |
            stdexec::let_value(
                [&](Request& request)
                {
                    return start_rpc(rpc, request, response, agrpc::use_sender);
                }) |
            stdexec::let_value(
                [&](bool)
                {
                    return rpc.finish();
                }) |
            stdexec::then(
                [&](const grpc::Status&)
                {
                    server_shutdown.initiate();
                }));
    CHECK_FALSE(is_cancelled);
}