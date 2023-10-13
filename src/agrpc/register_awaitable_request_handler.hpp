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

#ifndef AGRPC_AGRPC_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

#include <agrpc/detail/register_awaitable_request_handler.hpp>

AGRPC_NAMESPACE_BEGIN()

template <class ServerRPC, class RequestHandler, class CompletionToken>
auto register_awaitable_request_handler(const typename ServerRPC::executor_type& executor,
                                        detail::GetServerRPCServiceT<ServerRPC>& service,
                                        RequestHandler request_handler, CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void(std::exception_ptr)>(
        detail::AwaitableRequestHandlerInitiator<ServerRPC>{}, token, executor, service,
        static_cast<RequestHandler&&>(request_handler));
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP
