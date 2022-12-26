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
#include "helper.hpp"

#include <agrpc/asio_grpc.hpp>
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

// begin-snippet: client-side-unifex-unary
// ---------------------------------------------------
// A simple unary request with unifex coroutines.
// ---------------------------------------------------
// end-snippet
unifex::task<void> make_unary_request(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncUnary>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(42);
    example::v1::Response response;
    const auto status = co_await RPC::request(grpc_context, stub, client_context, request, response);

    abort_if_not(status.ok());
}
// ---------------------------------------------------
//

// begin-snippet: client-side-unifex-server-streaming
// ---------------------------------------------------
// A simple server-streaming request with unifex coroutines.
// ---------------------------------------------------
// end-snippet
unifex::task<void> make_server_streaming_request(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncServerStreaming>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(10);
    auto rpc = co_await RPC::request(grpc_context, stub, client_context, request);
    abort_if_not(rpc.ok());

    example::v1::Response response;
    while (co_await rpc.read(response))
    {
        std::cout << "Server streaming: " << response.integer() << '\n';
    }

    abort_if_not(rpc.ok());
}
// ---------------------------------------------------
//

// begin-snippet: client-side-unifex-with-deadline
// ---------------------------------------------------
// A unifex, unary request with a per-RPC step timeout. Using a unary RPC for demonstration purposes, the same mechanism
// can be applied to streaming RPCs, where it is arguably more useful. For unary RPCs,
// `grpc::ClientContext::set_deadline` is the preferred way of specifying a timeout.
// ---------------------------------------------------
// end-snippet
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

unifex::task<void> make_and_cancel_unary_request(agrpc::GrpcContext& grpc_context, example::v1::ExampleExt::Stub& stub)
{
    using RPC = agrpc::RPC<&example::v1::ExampleExt::Stub::PrepareAsyncSlowUnary>;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    example::v1::SlowRequest request;
    request.set_delay(2000);  // tell server to delay response by 2000ms
    google::protobuf::Empty response;

    grpc::Alarm alarm;
    const auto status = co_await run_with_deadline(alarm, grpc_context, client_context,
                                                   std::chrono::system_clock::now() + std::chrono::milliseconds(100),
                                                   RPC::request(grpc_context, stub, client_context, request, response));

    abort_if_not(grpc::StatusCode::CANCELLED == status.error_code());
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    example::v1::Example::Stub stub{channel};
    example::v1::ExampleExt::Stub stub_ext{channel};
    agrpc::GrpcContext grpc_context;

    grpc_context.work_started();
    unifex::sync_wait(unifex::when_all(
        unifex::finally(
            unifex::when_all(make_unary_request(grpc_context, stub), make_server_streaming_request(grpc_context, stub),
                             make_and_cancel_unary_request(grpc_context, stub_ext)),
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