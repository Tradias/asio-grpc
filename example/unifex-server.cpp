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

#include "example/v1/example.grpc.pb.h"
#include "example/v1/example_ext.grpc.pb.h"
#include "grpc/health/v1/health.grpc.pb.h"
#include "helper.hpp"
#include "server_shutdown_unifex.hpp"

#include <agrpc/asio_grpc.hpp>
#include <agrpc/health_check_service.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <unifex/finally.hpp>
#include <unifex/just_from.hpp>
#include <unifex/just_void_or_done.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/with_query_value.hpp>

// Example showing some of the features of using asio-grpc with libunifex.

using ExampleService = example::v1::Example::AsyncService;

using UnaryRPC = agrpc::ServerRPC<&ExampleService::RequestUnary>;
using ServerStreamingRPC = agrpc::ServerRPC<&ExampleService::RequestServerStreaming>;

// begin-snippet: server-side-unifex-unary
// ---------------------------------------------------
// Register a request handler to unary requests. A bit of boilerplate code regarding stop_source has been added to make
// the example testable.
// ---------------------------------------------------
// end-snippet
auto register_unary_request_handler(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    // Register a handler for all incoming RPCs of this unary method until the server is being shut down.
    return agrpc::register_sender_rpc_handler<UnaryRPC>(grpc_context, service,
                                                        [&](UnaryRPC& rpc, const UnaryRPC::Request& request)
                                                        {
                                                            return unifex::let_value_with(
                                                                []
                                                                {
                                                                    return UnaryRPC::Response{};
                                                                },
                                                                [&](auto& response)
                                                                {
                                                                    response.set_integer(request.integer());
                                                                    return rpc.finish(response, grpc::Status::OK);
                                                                });
                                                        });
}
// ---------------------------------------------------
//

// begin-snippet: server-side-unifex-server-streaming
// ---------------------------------------------------
// A simple server-streaming request handler using coroutines.
// ---------------------------------------------------
// end-snippet
auto handle_server_streaming_request(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    return agrpc::register_sender_rpc_handler<ServerStreamingRPC>(
        grpc_context, service,
        [&](ServerStreamingRPC& rpc, const ServerStreamingRPC::Request& request) -> unifex::task<void>
        {
            for (google::protobuf::int32 i = 0; i < request.integer(); ++i)
            {
                example::v1::Response response;
                response.set_integer(i);
                if (!co_await rpc.write(response))
                {
                    // The client hung up.
                    co_return;
                }
            }
            co_await rpc.finish(grpc::Status::OK);
        });
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The SlowUnary endpoint is used by the client to demonstrate per-RPC step cancellation. See unifex-client.cpp.
// ---------------------------------------------------
auto handle_slow_unary_request(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::AsyncService& service)
{
    using RPC = agrpc::ServerRPC<&example::v1::ExampleExt::AsyncService::RequestSlowUnary>;
    return agrpc::register_sender_rpc_handler<RPC>(
        grpc_context, service,
        [&](RPC& rpc, const RPC::Request& request) -> unifex::task<void>
        {
            agrpc::Alarm alarm{grpc_context};
            co_await alarm.wait(std::chrono::system_clock::now() + std::chrono::milliseconds(request.delay()));
            co_await rpc.finish({}, grpc::Status::OK);
        });
}
// ---------------------------------------------------
//

auto register_shutdown_request_handler(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::AsyncService& service,
                                       example::ServerShutdown& server_shutdown)
{
    using RPC = agrpc::ServerRPC<&example::v1::ExampleExt::AsyncService::RequestShutdown>;
    auto rpc_handler = [&](RPC& rpc, const RPC::Request&)
    {
        return unifex::let_value_with(
                   []
                   {
                       return RPC::Response{};
                   },
                   [&](auto& response)
                   {
                       return rpc.finish(response, grpc::Status::OK);
                   }) |
               unifex::then(
                   [&](bool)
                   {
                       server_shutdown.shutdown();
                   });
    };
    return agrpc::register_sender_rpc_handler<RPC>(grpc_context, service, std::move(rpc_handler));
}

template <class Sender>
void run_grpc_context_for_sender(agrpc::GrpcContext& grpc_context, Sender&& sender)
{
    grpc_context.work_started();
    unifex::sync_wait(
        unifex::when_all(unifex::finally(std::forward<Sender>(sender), unifex::just_from(
                                                                           [&]
                                                                           {
                                                                               grpc_context.work_finished();
                                                                           })),
                         unifex::just_from(
                             [&]
                             {
                                 grpc_context.run();
                             })));
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    example::v1::Example::AsyncService service;
    example::v1::ExampleExt::AsyncService service_ext;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    builder.RegisterService(&service_ext);
    agrpc::add_health_check_service(builder);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});
    agrpc::start_health_check_service(*server, grpc_context);

    example::ServerShutdown server_shutdown{*server};

    run_grpc_context_for_sender(
        grpc_context, unifex::with_query_value(unifex::when_all(register_unary_request_handler(grpc_context, service),
                                                                handle_server_streaming_request(grpc_context, service),
                                                                handle_slow_unary_request(grpc_context, service_ext),
                                                                register_shutdown_request_handler(
                                                                    grpc_context, service_ext, server_shutdown)),
                                               unifex::get_scheduler, unifex::inline_scheduler{}));
}