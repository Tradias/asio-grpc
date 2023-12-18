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

#include "example/v1/example.grpc.pb.h"

#include <agrpc/asio_grpc.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <unifex/config.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/when_all.hpp>

/* [unifex-server-streaming-client-side] */
unifex::task<void> server_streaming_example(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    using RPC = agrpc::ClientRPC<&example::v1::Example::Stub::PrepareAsyncServerStreaming>;
    grpc::ClientContext client_context;
    RPC::Request request;
    RPC rpc{grpc_context};
    co_await rpc.start(stub, request, agrpc::use_sender);
    RPC::Response response;
    co_await rpc.read(response, agrpc::use_sender);
    co_await rpc.finish(agrpc::use_sender);
}
/* [unifex-server-streaming-client-side] */

int main()
{
    auto stub =
        example::v1::Example::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context;

    unifex::sync_wait(unifex::when_all(server_streaming_example(grpc_context, *stub),
                                       [&]() -> unifex::task<void>
                                       {
                                           grpc_context.run();
                                           co_return;
                                       }()));
}