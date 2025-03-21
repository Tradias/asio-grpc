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

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;

// begin-snippet: client-side-multi-threaded

// Multi-threaded client using multiple GrpcContexts

// end-snippet

// A simple round robin strategy for picking the next GrpcContext to use for an RPC.
template <class Iterator>
class RoundRobin
{
  public:
    RoundRobin(Iterator begin, std::size_t size) : begin(begin), size(size) {}

    decltype(auto) next()
    {
        const auto cur = current.fetch_add(1, std::memory_order_relaxed);
        const auto pos = cur % size;
        return *std::next(begin, pos);
    }

  private:
    Iterator begin;
    std::size_t size;
    std::atomic_size_t current{};
};

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

struct GuardedGrpcContext
{
    agrpc::GrpcContext context;
    asio::executor_work_guard<agrpc::GrpcContext::executor_type> guard{context.get_executor()};
};

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;
    const auto thread_count = std::thread::hardware_concurrency();

    helloworld::Greeter::Stub stub{grpc::CreateChannel(host, grpc::InsecureChannelCredentials())};
    std::vector<std::unique_ptr<GuardedGrpcContext>> grpc_contexts;

    // Create GrpcContexts and their work guards.
    for (size_t i = 0; i < thread_count; ++i)
    {
        grpc_contexts.emplace_back(std::make_unique<GuardedGrpcContext>());
    }

    // Create one thread per GrpcContext.
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(
            [&, i]
            {
                grpc_contexts[i]->context.run();
            });
    }

    // Make some example requests.
    RoundRobin round_robin_grpc_contexts{grpc_contexts.begin(), thread_count};
    for (size_t i{}; i < 20; ++i)
    {
        auto& grpc_context = round_robin_grpc_contexts.next()->context;
        asio::co_spawn(grpc_context, make_request(grpc_context, stub), example::RethrowFirstArg{});
    }

    for (auto& grpc_context : grpc_contexts)
    {
        grpc_context->guard.reset();
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
}