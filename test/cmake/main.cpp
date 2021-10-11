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

#include "protos/test.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/spawn.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

int main()
{
    auto stub =
        agrpc::test::v1::Test::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    boost::asio::spawn(grpc_context.get_executor(),
                       [&](auto yield)
                       {
                           grpc::ClientContext client_context;
                           agrpc::test::v1::Request request;
                           request.set_integer(42);
                           auto reader =
                               stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
                           agrpc::read_initial_metadata(*reader, yield);
                           agrpc::test::v1::Response response;
                           grpc::Status status;
                           agrpc::finish(*reader, response, status, yield);
                       });

    grpc_context.run();
}