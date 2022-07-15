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

#include "example/v1/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value.hpp>

/* [repeatedly-request-sender] */
auto register_client_streaming_handler(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    return agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        [&](grpc::ServerContext&, example::v1::Request& request,
            grpc::ServerAsyncResponseWriter<example::v1::Response>& writer)
        {
            return unifex::let_value(unifex::just(example::v1::Response{}),
                                     [&](auto& response)
                                     {
                                         response.set_integer(request.integer());
                                         return agrpc::finish(writer, response, grpc::Status::OK,
                                                              agrpc::use_sender(grpc_context));
                                     });
        },
        agrpc::use_sender(grpc_context));
}
/* [repeatedly-request-sender] */
