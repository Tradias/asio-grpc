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

#include <agrpc/client_callback.hpp>
#include <agrpc/reactor_ptr.hpp>
#include <boost/asio/io_context.hpp>

namespace asio = boost::asio;
using error_code = boost::system::error_code;

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
