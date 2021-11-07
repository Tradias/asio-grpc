// Copyright 2021 Dennis Hezel
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

#include "helper.hpp"
#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/when_all.hpp>

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto stub = example::v1::Example::NewStub(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    grpc::Status status;

    unifex::sync_wait(unifex::when_all(
        [&]() -> unifex::task<void>
        {
            grpc::ClientContext client_context;
            client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
            example::v1::Request request;
            request.set_integer(42);
            const auto reader = stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
            example::v1::Response response;
            co_await agrpc::finish(*reader, response, status, agrpc::use_scheduler(grpc_context));
        }(),
        [&]() -> unifex::task<void>
        {
            grpc_context.run();
            co_return;
        }()));

    abort_if_not(status.ok());
}