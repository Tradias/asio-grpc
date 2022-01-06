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

#include "helper.hpp"
#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <unifex/just.hpp>
#include <unifex/stop_when.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

unifex::task<void> make_unary_request(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(42);
    const auto reader = stub.AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));

    example::v1::Response response;
    grpc::Status status;
    co_await agrpc::finish(*reader, response, status, agrpc::use_sender(grpc_context));

    abort_if_not(status.ok());
}

unifex::task<void> make_server_streaming_request(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(10);
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    abort_if_not(co_await agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context,
                                         request, reader, agrpc::use_sender(grpc_context)));

    example::v1::Response response;
    bool read_ok = co_await agrpc::read(*reader, response, agrpc::use_sender(grpc_context));
    while (read_ok)
    {
        std::cout << "Server streaming: " << response.integer() << '\n';
        read_ok = co_await agrpc::read(*reader, response, agrpc::use_sender(grpc_context));
    }

    grpc::Status status;
    co_await agrpc::finish(*reader, status, agrpc::use_sender(grpc_context));

    abort_if_not(status.ok());
}

template <class Sender>
auto run_with_deadline(grpc::Alarm& alarm, agrpc::GrpcContext& grpc_context, grpc::ClientContext& client_context,
                       std::chrono::system_clock::time_point deadline, Sender sender)
{
    return unifex::stop_when(std::move(sender),
                             unifex::then(agrpc::wait(alarm, deadline, agrpc::use_sender(grpc_context)),
                                          [&](bool wait_ok)
                                          {
                                              if (wait_ok)
                                              {
                                                  client_context.TryCancel();
                                              }
                                          }));
}

unifex::task<void> make_and_cancel_unary_request(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(2000);  // tell server to delay response by 2000ms
    auto reader = stub.AsyncSlowUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));

    example::v1::Response response;
    grpc::Status status;
    grpc::Alarm alarm;
    co_await run_with_deadline(alarm, grpc_context, client_context,
                               std::chrono::system_clock::now() + std::chrono::milliseconds(50),
                               agrpc::finish(*reader, response, status, agrpc::use_sender(grpc_context)));

    abort_if_not(grpc::StatusCode::CANCELLED == status.error_code());
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto stub = example::v1::Example::NewStub(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    unifex::sync_wait(unifex::when_all(make_unary_request(*stub, grpc_context),
                                       make_server_streaming_request(*stub, grpc_context),
                                       make_and_cancel_unary_request(*stub, grpc_context),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));
}