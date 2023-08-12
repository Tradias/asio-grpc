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

#ifndef AGRPC_DETAIL_UNSTARTED_SERVER_RPC_HPP
#define AGRPC_DETAIL_UNSTARTED_SERVER_RPC_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/server_rpc.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class RPC>
struct GetUnstartedServerRPCType;

template <auto RequestRPC, class Traits, class Executor>
struct GetUnstartedServerRPCType<agrpc::ServerRPC<RequestRPC, Traits, Executor>>
{
    using Type = detail::UnstartedServerRPC<RequestRPC, Traits, Executor>;
};

template <class ServerRPC>
using UnstartedServerRPCType = typename detail::GetUnstartedServerRPCType<ServerRPC>::Type;

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerUnaryRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class UnstartedServerRPC<RequestRPC, TraitsT, Executor> : public agrpc::ServerRPC<RequestRPC, TraitsT, Executor>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncResponseWriter<ResponseT>;

  public:
    using agrpc::ServerRPC<RequestRPC, TraitsT, Executor>::ServerRPC;

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, RequestT& request,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(),
            detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service, request},
            detail::ServerRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerClientStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class UnstartedServerRPC<RequestRPC, TraitsT, Executor> : public agrpc::ServerRPC<RequestRPC, TraitsT, Executor>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncReader<ResponseT, RequestT>;

  public:
    using agrpc::ServerRPC<RequestRPC, TraitsT, Executor>::ServerRPC;

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service},
            detail::ServerRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerServerStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class UnstartedServerRPC<RequestRPC, TraitsT, Executor> : public agrpc::ServerRPC<RequestRPC, TraitsT, Executor>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncWriter<ResponseT>;

  public:
    using agrpc::ServerRPC<RequestRPC, TraitsT, Executor>::ServerRPC;

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, RequestT& request,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(),
            detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service, request},
            detail::ServerRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerBidiStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class UnstartedServerRPC<RequestRPC, TraitsT, Executor> : public agrpc::ServerRPC<RequestRPC, TraitsT, Executor>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

  public:
    using agrpc::ServerRPC<RequestRPC, TraitsT, Executor>::ServerRPC;

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service},
            detail::ServerRequestSenderImplementation<grpc::ServerAsyncReaderWriter<ResponseT, RequestT>,
                                                      IS_NOTIFY_WHEN_DONE>{*this},
            token);
    }
};

template <class TraitsT, class Executor>
class UnstartedServerRPC<agrpc::ServerRPCType::GENERIC, TraitsT, Executor>
    : public agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC, TraitsT, Executor>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

  public:
    using agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC, TraitsT, Executor>::ServerRPC;

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(grpc::AsyncGenericService& service, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(),
            detail::ServerRequestSenderInitiation<agrpc::ServerRPCType::GENERIC, IS_NOTIFY_WHEN_DONE>{service},
            detail::ServerRequestSenderImplementation<grpc::GenericServerAsyncReaderWriter, IS_NOTIFY_WHEN_DONE>{*this},
            token);
    }
};

template <class Traits = agrpc::DefaultServerRPCTraits, class Executor = agrpc::GrpcExecutor>
using UnstartedGenericServerRPC = agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC, Traits, Executor>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_UNSTARTED_SERVER_RPC_HPP
