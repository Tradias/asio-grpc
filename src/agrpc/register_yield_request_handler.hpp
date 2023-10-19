// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_AGRPC_REGISTER_YIELD_REQUEST_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_YIELD_REQUEST_HANDLER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/register_yield_request_handler.hpp>

AGRPC_NAMESPACE_BEGIN()

template <class ServerRPC, class RequestHandler, class CompletionToken>
auto register_yield_request_handler(const typename ServerRPC::executor_type& executor,
                                    detail::GetServerRPCServiceT<ServerRPC>& service, RequestHandler&& request_handler,
                                    CompletionToken&& token)
{
    return asio::async_initiate<CompletionToken, void(std::exception_ptr)>(
        detail::RegisterYieldRequestHandlerInitiator<ServerRPC>{service}, token, executor,
        static_cast<RequestHandler&&>(request_handler));
}

template <class ServerRPC, class RequestHandler, class CompletionToken>
auto register_yield_request_handler(agrpc::GrpcContext& grpc_context, detail::GetServerRPCServiceT<ServerRPC>& service,
                                    RequestHandler&& request_handler, CompletionToken&& token)
{
    return agrpc::register_yield_request_handler<ServerRPC>(grpc_context.get_executor(), service,
                                                            static_cast<RequestHandler&&>(request_handler),
                                                            static_cast<CompletionToken&&>(token));
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REGISTER_YIELD_REQUEST_HANDLER_HPP
