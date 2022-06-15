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
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <unifex/finally.hpp>
#include <unifex/just.hpp>
#include <unifex/stop_when.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

// Example showing some of the features of using asio-grpc with libunifex.

// ---------------------------------------------------
// A simple unary request with coroutines.
// ---------------------------------------------------
unifex::task<void> make_unary_request(example::v1::Example::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(42);
    const auto reader =
        agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context, request, grpc_context);

    example::v1::Response response;
    grpc::Status status;
    co_await agrpc::finish(reader, response, status, agrpc::use_sender(grpc_context));

    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// A simple server-streaming request with coroutines.
// ---------------------------------------------------
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
    bool read_ok = co_await agrpc::read(reader, response, agrpc::use_sender(grpc_context));
    while (read_ok)
    {
        std::cout << "Server streaming: " << response.integer() << '\n';
        read_ok = co_await agrpc::read(reader, response, agrpc::use_sender(grpc_context));
    }

    grpc::Status status;
    co_await agrpc::finish(reader, status, agrpc::use_sender(grpc_context));

    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// A unary request with a per-RPC step timeout. Using a unary RPC for demonstration purposes, the same mechanism can be
// applied to streaming RPCs, where it is arguably more useful.
// For unary RPCs, `grpc::ClientContext::set_deadline` should be preferred.
// ---------------------------------------------------
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

unifex::task<void> make_and_cancel_unary_request(example::v1::ExampleExt::Stub& stub, agrpc::GrpcContext& grpc_context)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::SlowRequest request;
    request.set_delay(2000);  // tell server to delay response by 2000ms
    const auto reader =
        agrpc::request(&example::v1::ExampleExt::Stub::AsyncSlowUnary, stub, client_context, request, grpc_context);

    google::protobuf::Empty response;
    grpc::Status status;
    grpc::Alarm alarm;
    co_await run_with_deadline(alarm, grpc_context, client_context,
                               std::chrono::system_clock::now() + std::chrono::milliseconds(100),
                               agrpc::finish(reader, response, status, agrpc::use_sender(grpc_context)));

    abort_if_not(grpc::StatusCode::CANCELLED == status.error_code());
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto stub = example::v1::Example::NewStub(channel);
    const auto stub_ext = example::v1::ExampleExt::NewStub(channel);
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    grpc_context.work_started();
    unifex::sync_wait(
        unifex::when_all(unifex::finally(unifex::when_all(make_unary_request(*stub, grpc_context),
                                                          make_server_streaming_request(*stub, grpc_context),
                                                          make_and_cancel_unary_request(*stub_ext, grpc_context)),
                                         unifex::then(unifex::just(),
                                                      [&]
                                                      {
                                                          grpc_context.work_finished();
                                                      })),
                         unifex::then(unifex::just(),
                                      [&]
                                      {
                                          grpc_context.run();
                                      })));
}