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

#include "protos/benchmark_service.grpc.pb.h"
#include "server.h"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <forward_list>
#include <thread>

struct UnaryRPCContext
{
    grpc::ServerContext server_context;
    grpc::testing::SimpleRequest request;
    grpc::ServerAsyncResponseWriter<grpc::testing::SimpleResponse> writer{&server_context};
};

struct StreamingCallRPCContext
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncReaderWriter<grpc::testing::SimpleResponse, grpc::testing::SimpleRequest> reader_writer{
        &server_context};
};

template <class Function>
void co_spawn(agrpc::GrpcContext& grpc_context, Function&& function)
{
    boost::asio::co_spawn(grpc_context, std::forward<Function>(function), boost::asio::detached);
}

void repeatedly_request_unary(grpc::testing::BenchmarkService::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    co_spawn(grpc_context,
             [&]() -> boost::asio::awaitable<void>
             {
                 auto context = std::allocate_shared<UnaryRPCContext>(grpc_context.get_allocator());
                 bool request_ok =
                     co_await agrpc::request(&grpc::testing::BenchmarkService::AsyncService::RequestUnaryCall, service,
                                             context->server_context, context->request, context->writer);
                 if (!request_ok)
                 {
                     co_return;
                 }

                 co_spawn(grpc_context,
                          [&, context = std::move(context)]() -> boost::asio::awaitable<void>
                          {
                              grpc::testing::SimpleResponse response;
                              grpc::testing::Server::SetResponse(&context->request, &response);
                              co_await agrpc::finish(context->writer, response, grpc::Status::OK);
                          });
             });
}

void repeatedly_request_streaming_call(grpc::testing::BenchmarkService::AsyncService& service,
                                       agrpc::GrpcContext& grpc_context)
{
    co_spawn(grpc_context,
             [&]() -> boost::asio::awaitable<void>
             {
                 auto context = std::allocate_shared<StreamingCallRPCContext>(grpc_context.get_allocator());
                 bool request_ok =
                     co_await agrpc::request(&grpc::testing::BenchmarkService::AsyncService::RequestStreamingCall,
                                             service, context->server_context, context->reader_writer);
                 if (!request_ok)
                 {
                     co_return;
                 }

                 co_spawn(grpc_context,
                          [&, context = std::move(context)]() -> boost::asio::awaitable<void>
                          {
                              grpc::testing::SimpleRequest request;
                              grpc::testing::SimpleResponse response;
                              while (co_await agrpc::read(context->reader_writer, request))
                              {
                                  auto s = grpc::testing::Server::SetResponse(&request, &response);
                                  if (!s.ok())
                                  {
                                      co_await agrpc::finish(context->reader_writer, s);
                                      co_return;
                                  }
                                  bool write_ok = co_await agrpc::write(context->reader_writer, response);
                                  if (!write_ok)
                                  {
                                      co_await agrpc::finish(context->reader_writer, grpc::Status::OK);
                                      co_return;
                                  }
                              }
                              co_await agrpc::finish(context->reader_writer, grpc::Status::OK);
                          });
             });
}

int main()
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    grpc::testing::BenchmarkService::AsyncService service;

    const auto cpu_count = std::thread::hardware_concurrency();

    std::forward_list<agrpc::GrpcContext> grpc_contexts;
    for (size_t i = 0; i < cpu_count; ++i)
    {
        grpc_contexts.emplace_front(builder.AddCompletionQueue());
    }

    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    std::vector<std::thread> threads;
    for (size_t i = 0; i < cpu_count; ++i)
    {
        threads.emplace_back(
            [&, i]
            {
                auto& grpc_context = *std::next(grpc_contexts.begin(), i);
                repeatedly_request_unary(service, grpc_context);
                repeatedly_request_streaming_call(service, grpc_context);
                grpc_context.run();
            });
    }

    server->Shutdown();
    for (auto&& thread : threads)
    {
        thread.join();
    }
}