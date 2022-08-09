// Copyright 2022 Dennis Hezel
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

#ifndef AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
#define AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor, agrpc::RPCType = detail::RPC_TYPE<PrepareAsync>>
class BasicRPC;

namespace detail
{
template <class Responder>
struct ReadInitiateMetadataSenderImplementation : detail::GrpcSenderImplementationBase
{
    ReadInitiateMetadataSenderImplementation(agrpc::GrpcContext& grpc_context, Responder& responder,
                                             grpc::Status& status)
        : grpc_context(grpc_context), responder(responder), status(status)
    {
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { responder.ReadInitialMetadata(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (is_finished)
        {
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(ok);
            return;
        }
        is_finished = true;
        grpc_context.work_started();
        responder.Finish(&status, on_done.self());
    }

    agrpc::GrpcContext& grpc_context;
    Responder& responder;
    grpc::Status& status;
    bool is_finished{};
};

template <auto PrepareAsync, class Executor>
struct ClientUnaryRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::ClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync, class Executor>
struct ClientUnaryRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>;
    using Signature = void(RPC);

    ClientUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                           grpc::ClientContext& client_context, const Request& req,
                                           Response& response) noexcept
        : response(response),
          rpc(grpc_context.get_executor()),
          responder((stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue()))
    {
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept
    {
        responder->StartCall();
        responder->Finish(&response, &rpc.status(), self);
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(std::move(rpc));
    }

    Response& response;
    RPC rpc;
    std::unique_ptr<Responder<Response>> responder;
};

template <auto PrepareAsync, class Executor>
struct ClientServerStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
struct ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;
    using Signature = void(RPC);

    ClientServerStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, const Request& req) noexcept
        : rpc(grpc_context.get_executor())
    {
        rpc.responder = (stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue());
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder->StartCall(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (ok)
        {
            on_done(std::move(rpc));
        }
        else
        {
            rpc.grpc_context().work_started();
            rpc.responder->Finish(&rpc.status(), on_done.self());
        }
    }

    RPC rpc;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
