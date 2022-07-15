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
#include "example/v1/exampleExt.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <memory>
#include <optional>
#include <thread>

namespace asio = boost::asio;

// Example showing how to write to generic server that handles a single unary request.

struct GenericRequestHandler
{
    using executor_type = agrpc::GrpcContext::executor_type;

    agrpc::GrpcContext& grpc_context;

    static void handle_generic_request(grpc::GenericServerContext& server_context,
                                       grpc::GenericServerAsyncReaderWriter& reader_writer,
                                       const asio::yield_context& yield)
    {
        abort_if_not("/test.v1.Test/Unary" == server_context.method());

        grpc::ByteBuffer buffer;

        // -- Wait for the request message
        agrpc::read(reader_writer, buffer, yield);

        // -- Deserialize the request message
        example::v1::Request request;
        const auto deserialize_status =
            grpc::GenericDeserialize<grpc::ProtoBufferReader, example::v1::Request>(&buffer, &request);
        abort_if_not(deserialize_status.ok());
        abort_if_not(42 == request.integer());

        // -- Serialize the response message
        example::v1::Response response;
        response.set_integer(21);
        bool own_buffer;
        grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Response>(response, &buffer, &own_buffer);

        // -- Write the response message and finish this RPC with OK
        agrpc::write_and_finish(reader_writer, buffer, {}, grpc::Status::OK, yield);
    }

    void operator()(agrpc::GenericRepeatedlyRequestContext<>&& context) const
    {
        asio::spawn(grpc_context,
                    [context = std::move(context)](asio::yield_context yield)
                    {
                        handle_generic_request(context.server_context(), context.responder(), yield);
                    });
    }

    auto get_executor() const noexcept { return grpc_context.get_executor(); }
};

using ShutdownService = example::v1::ExampleExt::WithAsyncMethod_Shutdown<example::v1::ExampleExt::Service>;

void handle_shutdown_request(ShutdownService& shutdown_service, grpc::Server& server,
                             std::optional<std::thread>& shutdown_thread, const asio::yield_context& yield)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    google::protobuf::Empty request;
    agrpc::request(&ShutdownService::RequestShutdown, shutdown_service, server_context, request, writer, yield);
    agrpc::finish(writer, {}, grpc::Status::OK, yield);
    shutdown_thread.emplace(
        [&]
        {
            server.Shutdown();
        });
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;
    std::optional<std::thread> shutdown_thread;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());

    grpc::AsyncGenericService service;
    builder.RegisterAsyncGenericService(&service);

    // All requests will be handled in a generic fashion except the shutdown request:
    ShutdownService shutdown_service;
    builder.RegisterService(&shutdown_service);

    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    asio::spawn(grpc_context,
                [&](asio::yield_context yield)
                {
                    handle_shutdown_request(shutdown_service, *server, shutdown_thread, yield);
                });

    agrpc::repeatedly_request(service, GenericRequestHandler{grpc_context});

    grpc_context.run();
    if (shutdown_thread && shutdown_thread->joinable())
    {
        shutdown_thread->join();
    }
}