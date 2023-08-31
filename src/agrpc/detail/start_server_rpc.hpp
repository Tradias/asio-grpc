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

#ifndef AGRPC_DETAIL_START_SERVER_RPC_HPP
#define AGRPC_DETAIL_START_SERVER_RPC_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/server_rpc.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <auto RequestRPC, class TraitsT, class Executor, class Service,
          class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
auto start(agrpc::ServerRPC<RequestRPC, TraitsT, Executor>& rpc, Service& service,
           typename agrpc::ServerRPC<RequestRPC, TraitsT, Executor>::Request& request,
           CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
{
    using Responder = std::remove_reference_t<decltype(ServerRPCContextBaseAccess::responder(rpc))>;
    return detail::async_initiate_sender_implementation(
        RPCExecutorBaseAccess::grpc_context(rpc),
        detail::ServerRequestSenderInitiation<RequestRPC, TraitsT::NOTIFY_WHEN_DONE>{service, request},
        detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc}, token);
}

template <auto RequestRPC, class TraitsT, class Executor, class Service,
          class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
auto start(agrpc::ServerRPC<RequestRPC, TraitsT, Executor>& rpc, Service& service,
           CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
{
    using Responder = std::remove_reference_t<decltype(ServerRPCContextBaseAccess::responder(rpc))>;
    return detail::async_initiate_sender_implementation(
        RPCExecutorBaseAccess::grpc_context(rpc),
        detail::ServerRequestSenderInitiation<RequestRPC, TraitsT::NOTIFY_WHEN_DONE>{service},
        detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc}, token);
}

template <class TraitsT, class Executor, class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
auto start(agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC, TraitsT, Executor>& rpc, grpc::AsyncGenericService& service,
           CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
{
    return detail::async_initiate_sender_implementation(
        RPCExecutorBaseAccess::grpc_context(rpc),
        detail::ServerRequestSenderInitiation<agrpc::ServerRPCType::GENERIC, TraitsT::NOTIFY_WHEN_DONE>{service},
        detail::ServerRequestSenderImplementation<grpc::GenericServerAsyncReaderWriter, TraitsT::NOTIFY_WHEN_DONE>{rpc},
        token);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_START_SERVER_RPC_HPP
