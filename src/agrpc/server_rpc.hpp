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

#ifndef AGRPC_AGRPC_SERVER_RPC_HPP
#define AGRPC_AGRPC_SERVER_RPC_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/server_rpc_notify_when_done.hpp>
#include <agrpc/detail/server_rpc_notify_when_done_base.hpp>
#include <agrpc/detail/server_rpc_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

struct DefaultServerRPCTraits
{
    static constexpr bool NOTIFY_WHEN_DONE = true;
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerServerStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class ServerRPC<RequestRPC, TraitsT, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::ServerRPCBase<grpc::ServerAsyncWriter<ResponseT>, TraitsT::NOTIFY_WHEN_DONE>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncWriter<ResponseT>;

  public:
    using Service = ServiceT;
    using Request = RequestT;
    using Response = ResponseT;
    using Traits = TraitsT;

    explicit ServerRPC(const Executor& executor) : detail::RPCExecutorBase<Executor>{executor} {}

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, RequestT& request,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(),
            detail::ServerServerStreamingRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service, request},
            detail::ServerStreamingRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::WriteServerStreamingSenderInitiation<Responder>{*this, response},
            detail::WriteServerStreamingSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const grpc::Status& status, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishServerStreamingSenderInitation{status},
            detail::ServerFinishServerStreamingSenderImplementation<Responder>{*this}, token);
    }
};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_RPC_HPP
