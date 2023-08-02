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
#include <grpcpp/generic/generic_stub.h>

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

struct ServerRPCGrpcSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ServerContextCancellationFunction;
};

template <class Responder, bool IsNotifyWhenDone>
struct ServerStreamingRequestSenderImplementation : ServerRPCGrpcSenderImplementation
{
    using RPC = detail::ServerRPCBase<Responder, IsNotifyWhenDone>;

    explicit ServerStreamingRequestSenderImplementation(RPC& rpc) noexcept : rpc_(rpc) {}

    template <class OnDone>
    void done(OnDone on_done, bool ok) const
    {
        if (ok)
        {
            if constexpr (IsNotifyWhenDone)
            {
                on_done.grpc_context().work_started();
            }
            ServerRPCAccess::set_started(rpc_);
        }
        on_done(ok);
    }

    RPC& rpc_;
};

template <class Derived>
struct ServerStreamingRequestSenderInitiationBase
{
    template <class Responder, bool IsNotifyWhenDone>
    static auto& stop_function_arg(
        ServerStreamingRequestSenderImplementation<Responder, IsNotifyWhenDone>& impl) noexcept
    {
        return impl.rpc_.context();
    }

    template <class Responder, bool IsNotifyWhenDone>
    void initiate(agrpc::GrpcContext& grpc_context,
                  ServerStreamingRequestSenderImplementation<Responder, IsNotifyWhenDone>& impl, void* tag) const
    {
        auto& rpc = impl.rpc_;
        if constexpr (IsNotifyWhenDone)
        {
            ServerRPCAccess::initiate_notify_when_done(rpc, grpc_context);
            grpc_context.work_finished();
        }
        static_cast<const Derived&>(*this).do_initiate(grpc_context, impl.rpc_, tag);
    }
};

template <auto RequestRPC, bool IsNotifyWhenDone>
struct ServerServerStreamingRequestSenderInitiation;

template <class Service, class Request, class Response,
          detail::ServerServerStreamingRequest<Service, Request, Response> RequestRPC, bool IsNotifyWhenDone>
struct ServerServerStreamingRequestSenderInitiation<RequestRPC, IsNotifyWhenDone>
    : detail::ServerStreamingRequestSenderInitiationBase<
          ServerServerStreamingRequestSenderInitiation<RequestRPC, IsNotifyWhenDone>>
{
    ServerServerStreamingRequestSenderInitiation(Service& service, Request& req) noexcept : service_(service), req_(req)
    {
    }

    void do_initiate(agrpc::GrpcContext& grpc_context,
                     detail::ServerRPCBase<grpc::ServerAsyncWriter<Response>, IsNotifyWhenDone>& rpc, void* tag) const
    {
        (service_.*RequestRPC)(&rpc.context(), &req_, &ServerRPCAccess::responder(rpc),
                               grpc_context.get_completion_queue(), grpc_context.get_server_completion_queue(), tag);
    }

    Service& service_;
    Request& req_;
};

template <class Responder>
struct WriteServerStreamingSenderInitiation;

template <class Response, template <class> class Responder>
struct WriteServerStreamingSenderInitiation<Responder<Response>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ServerRPCAccess::responder(rpc_).Write(response_, tag);
    }

    detail::ServerRPCContextBase<Responder<Response>>& rpc_;
    const Response& response_;
};

using WriteServerStreamingSenderImplementation = ServerRPCGrpcSenderImplementation;

template <class Responder>
struct ServerFinishServerStreamingSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(bool);
    using StopFunction = detail::ServerContextCancellationFunction;

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        ServerRPCAccess::set_finished(rpc_);
        on_done(ok);
    }

    detail::ServerRPCContextBase<Responder>& rpc_;
};

struct ServerFinishServerStreamingSenderInitation
{
    template <class Responder>
    static auto& stop_function_arg(const ServerFinishServerStreamingSenderImplementation<Responder>& impl) noexcept
    {
        return impl.rpc_.context();
    }

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ServerFinishServerStreamingSenderImplementation<Responder>& impl,
                  void* tag) const
    {
        ServerRPCAccess::responder(impl.rpc_).Finish(status_, tag);
    }

    const grpc::Status& status_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_SENDER_HPP
