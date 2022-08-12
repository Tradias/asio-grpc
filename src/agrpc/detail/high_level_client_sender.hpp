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
#include <agrpc/detail/high_level_client.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/generic/generic_stub.h>

AGRPC_NAMESPACE_BEGIN()

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor, agrpc::RPCType = detail::RPC_TYPE<PrepareAsync>>
class BasicRPC;

namespace detail
{
enum class GenericRPCType
{
    UNARY,
    STREAMING
};

template <auto PrepareAsync, class Executor>
struct ClientUnaryRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::ClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync, class Executor>
struct ClientUnaryRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>;
    using Signature = void(RPC);

    struct Initiation
    {
        Response& response;
    };

    ClientUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                           grpc::ClientContext& client_context, const Request& req)
        : rpc(grpc_context.get_executor()),
          responder((stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue()))
    {
    }

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        responder->StartCall();
        responder->Finish(&init->response, &rpc.status(), init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(std::move(rpc));
    }

    RPC rpc;
    std::unique_ptr<Responder<Response>> responder;
};

template <class Executor>
struct GenericClientUnaryRequestSenderImplementation : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<detail::GenericRPCType::UNARY, Executor, agrpc::RPCType::CLIENT_UNARY>;
    using Signature = void(RPC);

    struct Initiation
    {
        grpc::ByteBuffer& response;
    };

    GenericClientUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, const std::string& method,
                                                  grpc::GenericStub& stub, grpc::ClientContext& client_context,
                                                  const grpc::ByteBuffer& req)
        : rpc(grpc_context.get_executor()),
          responder(stub.PrepareUnaryCall(&client_context, method, req, grpc_context.get_completion_queue()))
    {
    }

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        responder->StartCall();
        responder->Finish(&init->response, &rpc.status(), init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(std::move(rpc));
    }

    RPC rpc;
    std::unique_ptr<grpc::GenericClientAsyncResponseReader> responder;
};

template <auto PrepareAsync, class Executor>
struct ClientClientStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
struct ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>;
    using Signature = void(RPC);
    using Initiation = detail::Empty;

    ClientClientStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, Response& response)
        : rpc(grpc_context.get_executor())
    {
        rpc.responder() = (stub.*PrepareAsync)(&client_context, &response, grpc_context.get_completion_queue());
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder()->StartCall(self); }

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
            rpc.responder()->Finish(&rpc.status(), on_done.self());
        }
    }

    RPC rpc;
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
    using Initiation = detail::Empty;

    ClientServerStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, const Request& req)
        : rpc(grpc_context.get_executor())
    {
        rpc.responder() = (stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue());
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder()->StartCall(self); }

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
            rpc.responder()->Finish(&rpc.status(), on_done.self());
        }
    }

    RPC rpc;
};

template <auto PrepareAsync, class Executor>
struct ClientBidirectionalStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
struct ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>
    : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;
    using Signature = void(RPC);
    using Initiation = detail::Empty;

    ClientBidirectionalStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                            grpc::ClientContext& client_context)
        : rpc(grpc_context.get_executor())
    {
        rpc.responder() = (stub.*PrepareAsync)(&client_context, grpc_context.get_completion_queue());
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder()->StartCall(self); }

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
            rpc.responder()->Finish(&rpc.status(), on_done.self());
        }
    }

    RPC rpc;
};

template <class BasicRPCBase>
struct ReadInitiateMetadataSenderImplementation : detail::GrpcSenderImplementationBase
{
    using Initiation = detail::Empty;

    ReadInitiateMetadataSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder()->ReadInitialMetadata(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (is_finished)
        {
            rpc.responder().reset();
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        is_finished = true;
        rpc.grpc_context().work_started();
        rpc.responder()->Finish(&rpc.status(), on_done.self());
    }

    BasicRPCBase& rpc;
    bool is_finished{};
};

template <class Request, class Responder, class Executor>
struct ReadServerStreamingSenderImplementation;

template <class Request, template <class> class Responder, class Response, class Executor>
struct ReadServerStreamingSenderImplementation<Request, Responder<Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCClientServerStreamingBase<Request, Responder<Response>, Executor>;

    struct Initiation
    {
        Response& response;
    };

    ReadServerStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        rpc.responder()->Read(&init->response, init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (is_finished)
        {
            // TODO: does server-streaming have finish function?
            rpc.responder().reset();
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        is_finished = true;
        rpc.grpc_context().work_started();
        rpc.responder()->Finish(&rpc.status(), on_done.self());
    }

    BasicRPCBase& rpc;
    bool is_finished{};
};

template <class Responder, class Response, class Executor>
struct WriteClientStreamingSenderImplementation;

template <class Request, template <class> class Responder, class Response, class Executor>
struct WriteClientStreamingSenderImplementation<Responder<Request>, Response, Executor>
    : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCClientClientStreamingBase<Response, Responder<Request>, Executor>;

    struct Initiation
    {
        const Request& req;
        grpc::WriteOptions options;
    };

    WriteClientStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        auto& [req, options] = init.initiation();
        is_last_message = options.is_last_message();
        rpc.responder()->Write(req, options, init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (is_finished)
        {
            rpc.responder().reset();
            on_done(rpc.ok());
            return;
        }
        if (is_last_message)
        {
            is_last_message = false;
            this->initiate_finish(on_done.self());
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        this->initiate_finish(on_done.self());
    }

    void initiate_finish(void* self)
    {
        is_finished = true;
        rpc.grpc_context().work_started();
        rpc.responder()->Finish(&rpc.status(), self);
    }

    BasicRPCBase& rpc;
    bool is_finished{};
    bool is_last_message{};
};

template <class Responder, class Response, class Executor>
struct FinishClientStreamingSenderImplementation;

template <class Request, template <class> class Responder, class Response, class Executor>
struct FinishClientStreamingSenderImplementation<Responder<Request>, Response, Executor>
    : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCClientClientStreamingBase<Response, Responder<Request>, Executor>;
    using Initiation = detail::Empty;

    FinishClientStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder()->WritesDone(self); }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        if (is_finished)
        {
            rpc.responder().reset();
            on_done(rpc.ok());
            return;
        }
        is_finished = true;
        rpc.grpc_context().work_started();
        rpc.responder()->Finish(&rpc.status(), on_done.self());
    }

    BasicRPCBase& rpc;
    bool is_finished{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
