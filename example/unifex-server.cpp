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

// begin-snippet: server-side-unifex-unary
// ---------------------------------------------------
// Register a request handler to unary requests. A bit of boilerplate code regarding stop_source has been added to make
// the example testable.
// ---------------------------------------------------
// end-snippet
auto register_unary_request_handler(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    auto request_handler = [&](grpc::ServerContext&, example::v1::Request& request,
                               grpc::ServerAsyncResponseWriter<example::v1::Response>& writer) -> unifex::task<void>
    {
        example::v1::Response response;
        response.set_integer(request.integer());
        co_await agrpc::finish(writer, response, grpc::Status::OK, agrpc::use_sender(grpc_context));
    };

    // Register a handler for all incoming RPCs of this method (Example::Unary) until the server is being
    // shut down.
    return agrpc::repeatedly_request(&example::v1::Example::AsyncService::RequestUnary, service, request_handler,
                                     agrpc::use_sender(grpc_context));
}
// ---------------------------------------------------
//

// begin-snippet: server-side-unifex-server-streaming
// ---------------------------------------------------
// A simple server-streaming request handler using coroutines.
// ---------------------------------------------------
// end-snippet
unifex::task<void> handle_server_streaming_request(agrpc::GrpcContext& grpc_context,
                                                   example::v1::Example::AsyncService& service)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncWriter<example::v1::Response> writer{&server_context};
    example::v1::Request request;
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestServerStreaming, service, server_context,
                                 request, writer, agrpc::use_sender(grpc_context)))
    {
        // The gRPC server is shutting down.
        co_return;
    }
    for (google::protobuf::int32 i = 0; i < request.integer(); ++i)
    {
        example::v1::Response response;
        response.set_integer(i);
        if (!co_await agrpc::write(writer, response, agrpc::use_sender(grpc_context)))
        {
            // The client hung up.
            co_return;
        }
    }
    co_await agrpc::finish(writer, grpc::Status::OK, agrpc::use_sender(grpc_context));
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The SlowUnary endpoint is used by the client to demonstrate per-RPC step cancellation. See unifex-client.cpp.
// ---------------------------------------------------
unifex::task<void> handle_slow_unary_request(agrpc::GrpcContext& grpc_context,
                                             example::v1::ExampleExt::AsyncService& service)
{
    grpc::ServerContext server_context;
    example::v1::SlowRequest request;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    if (!co_await agrpc::request(&example::v1::ExampleExt::AsyncService::RequestSlowUnary, service, server_context,
                                 request, writer, agrpc::use_sender(grpc_context)))
    {
        co_return;
    }

    grpc::Alarm alarm;
    co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::milliseconds(request.delay()),
                         agrpc::use_sender(grpc_context));

    co_await agrpc::finish(writer, {}, grpc::Status::OK, agrpc::use_sender(grpc_context));
}
// ---------------------------------------------------
//

auto handle_shutdown_request(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::AsyncService& service,
                             example::ServerShutdown& server_shutdown)
{
    struct Context
    {
        grpc::ServerContext server_context{};
        grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};

        Context() = default;
    };
    return unifex::let_value_with(
               []
               {
                   return Context{};
               },
               [&](Context& context)
               {
                   return unifex::let_value_with(
                              []
                              {
                                  return google::protobuf::Empty{};
                              },
                              [&](auto& request)
                              {
                                  return agrpc::request(&example::v1::ExampleExt::AsyncService::RequestShutdown,
                                                        service, context.server_context, request, context.writer,
                                                        agrpc::use_sender(grpc_context));
                              }) |
                          unifex::let_value(
                              [](bool ok)
                              {
                                  return unifex::just_void_or_done(ok);
                              }) |
                          unifex::then(
                              []
                              {
                                  return google::protobuf::Empty{};
                              }) |
                          unifex::let_value(
                              [&](const auto& response)
                              {
                                  return agrpc::finish(context.writer, response, grpc::Status::OK,
                                                       agrpc::use_sender(grpc_context));
                              });
               }) |
           unifex::then(
               [&](bool)
               {
                   server_shutdown.shutdown();
               });
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

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    example::v1::Example::AsyncService service;
    builder.RegisterService(&service);
    example::v1::ExampleExt::AsyncService service_ext;
    builder.RegisterService(&service_ext);
    agrpc::add_health_check_service(builder);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});
    agrpc::start_health_check_service(*server, grpc_context);

    example::ServerShutdown server_shutdown{*server};

    run_grpc_context_for_sender(
        grpc_context,
        unifex::with_query_value(unifex::when_all(register_unary_request_handler(grpc_context, service),
                                                  handle_server_streaming_request(grpc_context, service),
                                                  handle_slow_unary_request(grpc_context, service_ext),
                                                  handle_shutdown_request(grpc_context, service_ext, server_shutdown)),
                                 unifex::get_scheduler, unifex::inline_scheduler{}));
}