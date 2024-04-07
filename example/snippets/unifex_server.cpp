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

#include "example/v1/example.grpc.pb.h"

#include <agrpc/asio_grpc.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value.hpp>
#include <unifex/let_value_with.hpp>

/* [server-rpc-unary-sender] */
auto server_rpc_unary_sender(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    using RPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestUnary>;
    return agrpc::register_sender_rpc_handler<RPC>(grpc_context, service,
                                                   [](RPC& rpc, RPC::Request& request)
                                                   {
                                                       return unifex::let_value_with(
                                                           []
                                                           {
                                                               return RPC::Response{};
                                                           },
                                                           [&](auto& response)
                                                           {
                                                               response.set_integer(request.integer());
                                                               return rpc.finish(response, grpc::Status::OK);
                                                           });
                                                   });
}
/* [server-rpc-unary-sender] */
