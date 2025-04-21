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

#include "helloworld/helloworld.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"

#include <agrpc/client_rpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;

// begin-snippet: client-side-multi-threaded-alternative

// Multi-threaded client using single a GrpcContext

// end-snippet

asio::awaitable<void> make_request(agrpc::GrpcContext& grpc_context, helloworld::Greeter::Stub& stub)
{
    using RPC = agrpc::ClientRPC<&helloworld::Greeter::Stub::PrepareAsyncSayHello>;
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    RPC::Request request;
    request.set_name("world");
    RPC::Response response;
    const auto status = co_await RPC::request(grpc_context, stub, client_context, request, response);

    abort_if_not(status.ok());
}

int main(int argc, const char** argv)
{
    const char* port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;
    const auto thread_count = std::thread::hardware_concurrency();

    helloworld::Greeter::Stub stub{grpc::CreateChannel(host, grpc::InsecureChannelCredentials())};

    // Create GrpcContext and work guard.
    agrpc::GrpcContext grpc_context(thread_count);
    auto guard = asio::make_work_guard(grpc_context);

    // Create threads.
    std::vector<std::thread> threads(thread_count);
    for (auto& thread : threads)
    {
        thread = std::thread(
            [&]
            {
                grpc_context.run();
            });
    }

    // Make some example requests.
    for (size_t i{}; i < 20; ++i)
    {
        asio::co_spawn(grpc_context, make_request(grpc_context, stub), example::RethrowFirstArg{});
    }

    guard.reset();
    for (auto& thread : threads)
    {
        thread.join();
    }
}