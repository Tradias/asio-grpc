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
#include "utils/time.hpp"

#include <stdexec/execution.hpp>

template <class... Sender>
void run(agrpc::GrpcContext& grpc_context, Sender&&... sender)
{
    grpc_context.work_started();
    stdexec::sync_wait(stdexec::when_all(stdexec::then(stdexec::when_all(std::forward<Sender>(sender)...),
                                                       [&](auto&&...)
                                                       {
                                                           grpc_context.work_finished();
                                                       }),
                                         stdexec::then(stdexec::just(),
                                                       [&]
                                                       {
                                                           grpc_context.run();
                                                       })));
}

TEST_CASE_FIXTURE(test::ClientServerRPCTest<test::UnaryClientRPC>,
                  "stdexec Unary ClientRPC::request automatically finishes rpc on error")
{
    server->Shutdown();
    client_context.set_deadline(test::ten_milliseconds_from_now());
    ClientRPC::Request request;
    ClientRPC::Response response;
    auto s = stdexec::then(request_rpc(true, client_context, request, response, agrpc::use_sender),
                           [](const auto& status)
                           {
                               const auto status_code = status.error_code();
                               CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == status_code ||
                                              grpc::StatusCode::UNAVAILABLE == status_code),
                                             status_code);
                           });
    run(grpc_context, std::move(s));
    grpc_context.run();
}