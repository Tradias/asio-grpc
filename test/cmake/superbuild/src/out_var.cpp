// Copyright 2025 Dennis Hezel
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

#include "out_var/msg/message.grpc.pb.h"
#include "out_var/subdir/other.1.grpc.pb.h"
#include "out_var/v1/out_var.grpc.pb.h"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <grpcpp/completion_queue.h>

void run_out_var()
{
    agrpc::GrpcContext grpc_context;

    out_var::v1::Test::AsyncService service;

    auto out_var_v1_rpc = &out_var::v1::Test::AsyncService::RequestUnary;
    auto other_rpc = &other::Other::AsyncService::RequestUnary;

    grpc::ServerContext server_context;

    out_var::msg::Request request;
    request.set_integer(42);

    using RPC = agrpc::ServerRPC<&out_var::v1::Test::AsyncService::RequestUnary>;
    RPC::Response response;
    auto cb = [](bool) {};
    const auto is_void = std::is_same_v<void, decltype(std::declval<RPC>().finish(response, {}, cb))>;

    grpc_context.run();
}