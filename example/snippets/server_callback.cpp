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

#include <agrpc/reactor_ptr.hpp>
#include <agrpc/server_callback.hpp>
#include <boost/asio/io_context.hpp>

namespace asio = boost::asio;
using error_code = boost::system::error_code;

struct ExampleService final : example::v1::Example::CallbackService
{
    using executor_type = asio::io_context::executor_type;

    explicit ExampleService(asio::io_context& io_context) : io_context_(io_context) {}

    /* [server-rpc-unary-callback] */
    grpc::ServerUnaryReactor* Unary(grpc::CallbackServerContext* context, const example::v1::Request*,
                                    example::v1::Response* response)
    {
        auto ptr = agrpc::make_reactor<agrpc::ServerUnaryReactor>(get_executor());
        auto& rpc = *ptr;
        context->AddInitialMetadata("example", "value");
        rpc.initiate_send_initial_metadata();
        rpc.wait_for_send_initial_metadata(
            [response, ptr = std::move(ptr)](const error_code&, bool ok)
            {
                if (!ok)
                {
                    return;
                }
                response->set_integer(42);
                ptr->initiate_finish(grpc::Status::OK);
                ptr->wait_for_finish(
                    [](const error_code&, bool ok)
                    {
                        if (!ok)
                        {
                            // ...
                        }
                    });
            });
        return rpc.get();
    }
    /* [server-rpc-unary-callback] */

    /* [server-rpc-client-streaming-callback] */
    grpc::ServerReadReactor<example::v1::Request>* ClientStreaming(grpc::CallbackServerContext*,
                                                                   example::v1::Response* response)
    {
        auto ptr = agrpc::make_reactor<agrpc::ServerReadReactor<example::v1::Request>>(get_executor());
        auto& rpc = *ptr;
        auto request = std::make_unique<example::v1::Request>();
        rpc.initiate_read(*request);
        rpc.wait_for_read(
            [response, ptr = std::move(ptr), request = std::move(request)](const error_code&, bool ok)
            {
                if (!ok)
                {
                    return;
                }
                response->set_integer(request->integer());
                ptr->initiate_finish(grpc::Status::OK);
            });
        return rpc.get();
    }
    /* [server-rpc-client-streaming-callback] */

    executor_type get_executor() noexcept { return io_context_.get_executor(); }

    asio::io_context& io_context_;
};
