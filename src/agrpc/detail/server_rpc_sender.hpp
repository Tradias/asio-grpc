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

#ifndef AGRPC_DETAIL_SERVER_RPC_SENDER_HPP
#define AGRPC_DETAIL_SERVER_RPC_SENDER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/generic/async_generic_service.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using ServerRPCAccess = ServerRPCContextBaseAccess;

struct ServerContextCancellationFunction
{
#if !defined(AGRPC_UNIFEX)
    explicit
#endif
        ServerContextCancellationFunction(grpc::ServerContext& server_context) noexcept
        : server_context_(server_context)
    {
    }

    void operator()() const { server_context_.TryCancel(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type) const
    {
        if (static_cast<bool>(type & (asio::cancellation_type::terminal | asio::cancellation_type::partial)))
        {
            operator()();
        }
    }
#endif

    grpc::ServerContext& server_context_;
};

template <class Responder, bool IsNotifyWhenDone>
struct ServerRequestSenderImplementation : detail::GrpcSenderImplementationBase
{
    using RPC = detail::ServerRPCResponderAndNotifyWhenDone<Responder, IsNotifyWhenDone>;

    explicit ServerRequestSenderImplementation(RPC& rpc) noexcept : rpc_(rpc) {}

    void complete(agrpc::GrpcContext& grpc_context, bool ok) const noexcept
    {
        if (ok)
        {
            if constexpr (IsNotifyWhenDone)
            {
                grpc_context.work_started();
            }
        }
    }

    RPC& rpc_;
};

template <auto RequestRPC, bool IsNotifyWhenDone>
struct ServerRequestSenderInitiation;

template <class Service, class Responder, detail::ServerSingleArgRequest<Service, Responder> RequestRPC,
          bool IsNotifyWhenDone>
struct ServerRequestSenderInitiation<RequestRPC, IsNotifyWhenDone>
{
    void initiate(agrpc::GrpcContext& grpc_context,
                  ServerRequestSenderImplementation<Responder, IsNotifyWhenDone>& impl, void* tag) const
    {
        auto& rpc = impl.rpc_;
        ServerRPCAccess::initiate_notify_when_done(rpc);
        (service_.*RequestRPC)(&rpc.context(), &ServerRPCAccess::responder(rpc), grpc_context.get_completion_queue(),
                               grpc_context.get_server_completion_queue(), tag);
    }

    Service& service_;
};

template <class Service, class Request, class Responder,
          detail::ServerMultiArgRequest<Service, Request, Responder> RequestRPC, bool IsNotifyWhenDone>
struct ServerRequestSenderInitiation<RequestRPC, IsNotifyWhenDone>
{
    void initiate(agrpc::GrpcContext& grpc_context,
                  ServerRequestSenderImplementation<Responder, IsNotifyWhenDone>& impl, void* tag) const
    {
        auto& rpc = impl.rpc_;
        ServerRPCAccess::initiate_notify_when_done(rpc);
        (service_.*RequestRPC)(&rpc.context(), &req_, &ServerRPCAccess::responder(rpc),
                               grpc_context.get_completion_queue(), grpc_context.get_server_completion_queue(), tag);
    }

    Service& service_;
    Request& req_;
};

template <bool IsNotifyWhenDone>
struct ServerRequestSenderInitiation<agrpc::ServerRPCType::GENERIC, IsNotifyWhenDone>
{
    void initiate(agrpc::GrpcContext& grpc_context,
                  ServerRequestSenderImplementation<grpc::GenericServerAsyncReaderWriter, IsNotifyWhenDone>& impl,
                  void* tag) const
    {
        auto& rpc = impl.rpc_;
        ServerRPCAccess::initiate_notify_when_done(rpc);
        service_.RequestCall(&rpc.context(), &ServerRPCAccess::responder(rpc), grpc_context.get_completion_queue(),
                             grpc_context.get_server_completion_queue(), tag);
    }

    grpc::AsyncGenericService& service_;
};

struct ServerRPCGrpcSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ServerContextCancellationFunction;
};

struct ServerRPCSenderInitiationBase
{
    template <class Impl>
    static auto& stop_function_arg(Impl& impl) noexcept
    {
        return impl.rpc_.context();
    }
};

using SendInitialMetadataSenderImplementation = ServerRPCGrpcSenderImplementation;

template <class Responder>
struct SendInitialMetadataSenderInitiation
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ServerRPCAccess::responder(rpc_).SendInitialMetadata(tag);
    }

    detail::ServerRPCContextBase<Responder>& rpc_;
};

using ServerReadSenderImplementation = ServerRPCGrpcSenderImplementation;

template <class Responder>
struct ServerReadSenderInitiation;

template <class Response, class Request, template <class, class> class Responder>
struct ServerReadSenderInitiation<Responder<Response, Request>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const { ServerRPCAccess::responder(rpc_).Read(&request_, tag); }

    detail::ServerRPCContextBase<Responder<Response, Request>>& rpc_;
    Request& request_;
};

using ServerWriteSenderImplementation = ServerRPCGrpcSenderImplementation;

template <class Responder>
struct ServerWriteSenderInitiation;

template <class Response, template <class, class...> class Responder, class... Request>
struct ServerWriteSenderInitiation<Responder<Response, Request...>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ServerRPCAccess::responder(rpc_).Write(response_, options_, tag);
    }

    detail::ServerRPCContextBase<Responder<Response, Request...>>& rpc_;
    const Response& response_;
    grpc::WriteOptions options_;
};

template <class Responder>
struct ServerFinishSenderImplementation : ServerRPCGrpcSenderImplementation
{
    explicit ServerFinishSenderImplementation(detail::ServerRPCContextBase<Responder>& rpc) noexcept : rpc_(rpc) {}

    void complete(const agrpc::GrpcContext&, bool) noexcept { ServerRPCAccess::set_finished(rpc_); }

    detail::ServerRPCContextBase<Responder>& rpc_;
};

template <class Response>
struct ServerFinishWithMessageInitation : ServerRPCSenderInitiationBase
{
    ServerFinishWithMessageInitation(const Response& response, const grpc::Status& status) noexcept
        : response_(response), status_(status)
    {
    }

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ServerFinishSenderImplementation<Responder>& impl, void* tag) const
    {
        ServerRPCAccess::responder(impl.rpc_).Finish(response_, status_, tag);
    }

    const Response& response_;
    const grpc::Status& status_;
};

struct ServerFinishWithErrorSenderInitation : ServerRPCSenderInitiationBase
{
    explicit ServerFinishWithErrorSenderInitation(const grpc::Status& status) noexcept : status_(status) {}

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ServerFinishSenderImplementation<Responder>& impl, void* tag) const
    {
        ServerRPCAccess::responder(impl.rpc_).FinishWithError(status_, tag);
    }

    const grpc::Status& status_;
};

struct ServerFinishSenderInitation : ServerRPCSenderInitiationBase
{
    explicit ServerFinishSenderInitation(const grpc::Status& status) noexcept : status_(status) {}

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ServerFinishSenderImplementation<Responder>& impl, void* tag) const
    {
        ServerRPCAccess::responder(impl.rpc_).Finish(status_, tag);
    }

    const grpc::Status& status_;
};

template <class Response>
struct ServerWriteAndFinishSenderInitation : ServerRPCSenderInitiationBase
{
    ServerWriteAndFinishSenderInitation(const Response& response, const grpc::Status& status,
                                        grpc::WriteOptions options)
        : response_(response), status_(status), options_(options)
    {
    }

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ServerFinishSenderImplementation<Responder>& impl, void* tag) const
    {
        ServerRPCAccess::responder(impl.rpc_).WriteAndFinish(response_, options_, status_, tag);
    }

    const Response& response_;
    const grpc::Status& status_;
    grpc::WriteOptions options_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_SENDER_HPP
