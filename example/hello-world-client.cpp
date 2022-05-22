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

#include "helloworld/helloworld.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    grpc::Status status;

    // begin-snippet: client-side-helloworld
    const auto stub = helloworld::Greeter::NewStub(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            grpc::ClientContext client_context;
            helloworld::HelloRequest request;
            request.set_name("world");
            std::unique_ptr<grpc::ClientAsyncResponseReader<helloworld::HelloReply>> reader =
                stub->AsyncSayHello(&client_context, request, agrpc::get_completion_queue(grpc_context));
            helloworld::HelloReply response;
            co_await agrpc::finish(reader, response, status);
        },
        boost::asio::detached);

    grpc_context.run();
    // end-snippet

    abort_if_not(status.ok());
}