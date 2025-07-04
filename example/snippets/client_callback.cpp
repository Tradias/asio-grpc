// Copyright 2025 Dennis Hezel
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

#include <agrpc/client_callback.hpp>
#include <agrpc/reactor_ptr.hpp>
#include <boost/asio/io_context.hpp>

namespace asio = boost::asio;
using error_code = boost::system::error_code;

/* [client-rpc-unary-call] */
void unary_call(example::v1::Example::Stub& stub, example::v1::Request request)
{
    auto state = std::make_unique<std::tuple<example::v1::Request, grpc::ClientContext, example::v1::Response>>();
    auto& [req, context, rsp] = *state;
    req = std::move(request);
    agrpc::unary_call(&example::v1::Example::Stub::async::Unary, stub.async(), context, req, rsp,
                      [state = std::move(state)](const grpc::Status& /*status*/) {});
}
/* [client-rpc-unary-call] */

/* [client-rpc-unary-callback] */
void unary(asio::io_context& io_context, example::v1::Example::Stub& stub, const example::v1::Request& request)
{
    auto ptr = agrpc::make_reactor<agrpc::ClientUnaryReactor>(io_context.get_executor());
    auto& rpc = *ptr;
    auto response = std::make_unique<example::v1::Response>();
    rpc.start(&example::v1::Example::Stub::async::Unary, stub.async(), request, *response);
    rpc.wait_for_initial_metadata(
        [ptr = std::move(ptr), response = std::move(response)](const error_code&, bool ok) mutable
        {
            if (!ok)
            {
                return;
            }
            // Utilize the server's initial metadata:
            //   ptr->context().GetServerInitialMetadata()
            ptr->wait_for_finish(
                [response = std::move(response)](const error_code&, const grpc::Status& status)
                {
                    if (!status.ok())
                    {
                        // ...
                    }
                });
        });
}
/* [client-rpc-unary-callback] */

/* [client-rpc-client-streaming-callback] */
void client_streaming(asio::io_context& io_context, example::v1::Example::Stub& stub)
{
    auto ptr = agrpc::make_reactor<agrpc::ClientWriteReactor<example::v1::Request>>(io_context.get_executor());
    auto& rpc = *ptr;
    auto response = std::make_unique<example::v1::Response>();
    rpc.start(&example::v1::Example::Stub::async::ClientStreaming, stub.async(), *response);
    auto request = std::make_unique<example::v1::Request>();
    rpc.initiate_write(*request);
    rpc.wait_for_write(
        [ptr = std::move(ptr), request = std::move(request), response = std::move(response)](const error_code&,
                                                                                             bool ok) mutable
        {
            if (!ok)
            {
                return;
            }
            ptr->wait_for_finish(
                [response = std::move(response)](const error_code&, const grpc::Status& status)
                {
                    if (!status.ok())
                    {
                        // ...
                    }
                });
        });
}
/* [client-rpc-client-streaming-callback] */

/* [client-rpc-server-streaming-callback] */
void server_streaming(asio::io_context& io_context, example::v1::Example::Stub& stub)
{
    auto ptr = agrpc::make_reactor<agrpc::ClientReadReactor<example::v1::Response>>(io_context.get_executor());
    auto& rpc = *ptr;
    auto request = std::make_unique<example::v1::Request>();
    rpc.start(&example::v1::Example::Stub::async::ServerStreaming, stub.async(), *request);
    auto response = std::make_unique<example::v1::Response>();
    rpc.initiate_read(*response);
    rpc.wait_for_read(
        [ptr = std::move(ptr), request = std::move(request), response = std::move(response)](const error_code&,
                                                                                             bool ok) mutable
        {
            if (!ok)
            {
                return;
            }
            ptr->wait_for_finish(
                [request = std::move(request)](const error_code&, const grpc::Status& status)
                {
                    if (!status.ok())
                    {
                        // ...
                    }
                });
        });
}
/* [client-rpc-server-streaming-callback] */

/* [client-rpc-bidi-streaming-callback] */
void bidi_streaming(asio::io_context& io_context, example::v1::Example::Stub& stub)
{
    auto ptr = agrpc::make_reactor<agrpc::ClientBidiReactor<example::v1::Request, example::v1::Response>>(
        io_context.get_executor());
    auto& rpc = *ptr;
    rpc.start(&example::v1::Example::Stub::async::BidirectionalStreaming, stub.async());
    auto request = std::make_unique<example::v1::Request>();
    rpc.initiate_write(*request);
    rpc.wait_for_write(
        [ptr = std::move(ptr), request = std::move(request)](const error_code&, bool ok) mutable
        {
            if (!ok)
            {
                return;
            }
            auto& rpc = *ptr;
            auto response = std::make_unique<example::v1::Response>();
            rpc.initiate_read(*response);
            rpc.wait_for_read(
                [ptr = std::move(ptr), response = std::move(response)](const error_code&, bool ok)
                {
                    if (!ok)
                    {
                        return;
                    }
                    ptr->wait_for_finish(
                        [](const error_code&, const grpc::Status& status)
                        {
                            if (!status.ok())
                            {
                                // ...
                            }
                        });
                });
        });
}
/* [client-rpc-bidi-streaming-callback] */
