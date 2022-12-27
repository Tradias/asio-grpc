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

#include "../example/helper/buffer.hpp"
#include "test/v1/test.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/grpc_initiate.hpp>
#include <agrpc/grpc_stream.hpp>
#include <agrpc/high_level_client.hpp>
#include <agrpc/notify_when_done.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>

#include <cstddef>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <auto PrepareAsync, class Executor>
struct ServerServerStreamingRequestSenderImplementation;

template <class Service, class Request, class Response, template <class> class Responder,
          detail::ServerMultiArgRequest<Service, Request, Responder<Response>> PrepareAsync, class Executor>
struct ServerServerStreamingRequestSenderImplementation<PrepareAsync, Executor>
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(bool);
    using StopFunction = detail::Empty;

    struct Initiation
    {
        agrpc::RPC<PrepareAsync, Executor>& rpc;
        Service& service;
        Request& req;
    };

    // detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc; }

    void initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation, void* self) noexcept
    {
        auto& [rpc, service, req] = initiation;
        (service.*PrepareAsync)(&rpc.server_context(), &req, &rpc.responder(), grpc_context.get_completion_queue(),
                                grpc_context.get_server_completion_queue(), self);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }
};

template <class Responder>
struct WriteServerStreamingSenderImplementation;

template <class Response, template <class> class Responder>
struct WriteServerStreamingSenderImplementation<Responder<Response>> : detail::GrpcSenderImplementationBase
{
    struct Initiation
    {
        const Response& response;
    };

    WriteServerStreamingSenderImplementation(Responder<Response>& responder) : responder(responder) {}

    // detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc; }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, detail::OperationBase* operation) noexcept
    {
        responder.Write(initiation.response, operation);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    Responder<Response>& responder;
};
}

template <class ServiceT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::ServerMultiArgRequest<ServiceT, RequestT, ResponderT<ResponseT>> PrepareAsync, class Executor>
class RPC<PrepareAsync, Executor, agrpc::RPCType::SERVER_SERVER_STREAMING> : public detail::RPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;
    using Responder = ResponderT<ResponseT>;

    explicit RPC(const Executor& executor) : detail::RPCExecutorBase<Executor>{executor} {}

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto request(ServiceT& service, RequestT& request,
                 CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ServerServerStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            this->grpc_context(), {*this, service, request}, {}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::WriteServerStreamingSenderImplementation<Responder>>(this->grpc_context(), {response}, {responder_},
                                                                         token);
    }

    Responder& responder() noexcept { return responder_; }

    grpc::ServerContext& server_context() noexcept { return server_context_; }

  private:
    grpc::ServerContext server_context_;
    Responder responder_{&server_context_};
};

class NotifyWhenDone
{
  private:
    template <class CompletionToken>
    auto async_notify_when_done(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context,
                                CompletionToken&& token)
    {
        return agrpc::grpc_initiate(
            [&](const agrpc::GrpcContext&, void* tag)
            {
                server_context.AsyncNotifyWhenDone(tag);
            },
            asio::bind_executor(grpc_context, std::forward<CompletionToken>(token)));
    }

  public:
    void initiate(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context)
    {
        async_notify_when_done(grpc_context, server_context,
                               buffer.bind_allocator(
                                   [&](bool)
                                   {
                                       running = false;
                                       safe.token()();
                                   }));
    }

    [[nodiscard]] bool is_running() const noexcept { return running; }

    template <class CompletionToken = agrpc::DefaultCompletionToken>
    auto done(CompletionToken&& token = {})
    {
        return safe.wait(static_cast<CompletionToken&&>(token));
    }

  private:
    agrpc::CancelSafe<void()> safe;
    bool running{true};
    example::Buffer<32> buffer;
};

AGRPC_NAMESPACE_END

template <auto PrepareAsync, class Service, class RequestHandler>
asio::awaitable<void> request_loop(agrpc::GrpcContext& grpc_context, Service& service, RequestHandler request_handler)
{
    using RPC = agrpc::RPC<PrepareAsync>;
    RPC rpc{grpc_context.get_executor()};
    agrpc::NotifyWhenDone on_done;
    on_done.initiate(grpc_context, rpc.server_context());
    grpc_context.work_finished();
    typename RPC::Request request;
    const bool ok = co_await rpc.request(service, request);
    if (!ok)
    {
        co_return;
    }
    grpc_context.work_started();
    asio::co_spawn(grpc_context, request_loop<PrepareAsync>(grpc_context, service, request_handler), test::NoOp{});
    co_await request_handler(rpc, request, on_done);
    if (on_done.is_running())
    {
        co_await on_done.done();
    }
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitableaa server streaming")
{
    test::ServerShutdownInitiator shutdown{*server};
    test::co_spawn_and_run(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await request_loop<&test::v1::Test::AsyncService::RequestServerStreaming>(
                grpc_context, service,
                [&](auto& rpc, auto& request, auto&) -> asio::awaitable<void>
                {
                    CHECK_EQ(42, request.integer());
                    test::msg::Response response;
                    response.set_integer(21);

                    CHECK(co_await rpc.write(response));
                    // CHECK(co_await rpc.finish(grpc::Status::OK));
                    shutdown.initiate();
                    co_return;
                });
        },
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            request.set_integer(42);
            test::ClientAsyncReader<false> reader;
            CHECK(co_await agrpc::request(&test::v1::Test::Stub::PrepareAsyncServerStreaming, stub, client_context,
                                          request, reader));
            test::msg::Response response;
            CHECK(co_await agrpc::read(*reader, response));
            client_context.TryCancel();
            // grpc::Status status;
            // CHECK(co_await agrpc::finish(*reader, status));
            // CHECK(status.ok());
            // CHECK_EQ(21, response.integer());
        });
}