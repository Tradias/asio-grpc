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

#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <unifex/config.hpp>

#if !UNIFEX_NO_COROUTINES
#include <unifex/task.hpp>

// begin-snippet: unifex-server-streaming-client-side
unifex::task<void> unified_executors(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    test::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<test::v1::Response>> reader;
    co_await agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, stub, client_context, request, reader,
                            agrpc::use_sender(grpc_context));
    test::v1::Response response;
    co_await agrpc::read(*reader, response, agrpc::use_sender(grpc_context));
    grpc::Status status;
    co_await agrpc::finish(*reader, status, agrpc::use_sender(grpc_context));
}
// end-snippet
#endif

int main()
{
    auto stub =
        example::v1::Example::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

#if !UNIFEX_NO_COROUTINES
    unifex::sync_wait(unifex::when_all(unified_executors(*stub, grpc_context),
                                       [&]() -> unifex::task<void>
                                       {
                                           grpc_context.run();
                                           co_return;
                                       }()));
#endif
}