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

#include "target.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <grpcpp/completion_queue.h>

#if defined __has_include
#if !__has_include("target_mock.grpc.pb.h")
static_assert(false, "asio_grpc_protobuf_generate GENERATE_MOCK_CODE did not create expected output file");
#endif
#endif

void run_target()
{
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    auto rpc = &target::Test::AsyncService::RequestUnary;

    target::Request request;
    request.set_integer(42);

    target::Request response;

    grpc_context.run();
}