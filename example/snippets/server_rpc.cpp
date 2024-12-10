// Copyright 2024 Dennis Hezel
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

#include "example/v1/example.grpc.pb.h"

#include <agrpc/asio_grpc.hpp>
#include <agrpc/register_callback_rpc_handler.hpp>
#include <agrpc/register_yield_rpc_handler.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

/* [waiter-example] */
using ServerRPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestBidirectionalStreaming>;

asio::awaitable<void> rpc_handler_using_waiter(ServerRPC& rpc)
{
    ServerRPC::Request request;
    ServerRPC::Response response;

    agrpc::Alarm alarm{rpc.get_executor()};

    agrpc::Waiter<void(bool)> waiter;
    waiter.initiate(agrpc::read, rpc, request);

    auto next_deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);

    // Read requests from the client and send a response back every five seconds
    while (true)
    {
        auto [completion_order, read_error_code, read_ok, alarm_expired] =
            co_await asio::experimental::make_parallel_group(waiter.wait(asio::deferred),
                                                             alarm.wait(next_deadline, asio::deferred))
                .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
        if (0 == completion_order[0])  // read completed
        {
            if (!read_ok)
            {
                co_return;
            }
            waiter.initiate(agrpc::read, rpc, request);
        }
        else  // alarm expired
        {
            co_await rpc.write(response, asio::use_awaitable);
            next_deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        }
    }
}
/* [waiter-example] */

/* [server-rpc-generic] */
void server_rpc_generic(agrpc::GrpcContext& grpc_context, grpc::AsyncGenericService& service)
{
    using RPC = asio::use_awaitable_t<>::as_default_on_t<agrpc::GenericServerRPC>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC& rpc) -> asio::awaitable<void>
        {
            RPC::Request request_buffer;
            if (!co_await rpc.read(request_buffer))
            {
                co_return;
            }
            example::v1::Request request;
            if (const auto status =
                    grpc::GenericDeserialize<grpc::ProtoBufferReader, example::v1::Request>(&request_buffer, &request);
                !status.ok())
            {
                co_await rpc.finish(status);
                co_return;
            }
            example::v1::Response response;
            response.set_integer(request.integer());
            RPC::Response response_buffer;
            bool own_buffer;
            if (const auto status = grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Response>(
                    response, &response_buffer, &own_buffer);
                !status.ok())
            {
                co_await rpc.finish(status);
                co_return;
            }
            if (!co_await rpc.write(response_buffer))
            {
                co_return;
            }
            co_await rpc.finish(grpc::Status::OK);
        },
        asio::detached);
}
/* [server-rpc-generic] */

/* [server-rpc-unary-yield] */
void server_rpc_unary_yield(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    using RPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestUnary>;
    agrpc::register_yield_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC& rpc, RPC::Request& request, const asio::yield_context& yield)
        {
            RPC::Response response;
            response.set_integer(request.integer());
            rpc.finish(response, grpc::Status::OK, yield);
        },
        asio::detached);
}
/* [server-rpc-unary-yield] */

/* [server-rpc-unary-callback] */
void server_rpc_unary_callback(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    using RPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestUnary>;
    agrpc::register_callback_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC::Ptr ptr, RPC::Request& request)
        {
            RPC::Response response;
            response.set_integer(request.integer());
            auto& rpc = *ptr;
            rpc.finish(response, grpc::Status::OK, [p = std::move(ptr)](bool) {});
        },
        asio::detached);
}
/* [server-rpc-unary-callback] */

// Explicitly formatted using `ColumnLimit: 90`
// clang-format off
/* [server-rpc-unary] */
void server_rpc_unary(agrpc::GrpcContext& grpc_context,
                      example::v1::Example::AsyncService& service)
{
    using RPC = asio::use_awaitable_t<>::as_default_on_t<
        agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestUnary>>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC& rpc, RPC::Request& request) -> asio::awaitable<void>
        {
            RPC::Response response;
            response.set_integer(request.integer());
            co_await rpc.finish(response, grpc::Status::OK);

            // Alternatively finish with an error:
            co_await rpc.finish_with_error(grpc::Status::CANCELLED);
        },
        asio::detached);
}
/* [server-rpc-unary] */

/* [server-rpc-client-streaming] */
void server_rpc_client_streaming(agrpc::GrpcContext& grpc_context,
                                 example::v1::Example::AsyncService& service)
{
    using RPC = asio::use_awaitable_t<>::as_default_on_t<
        agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestClientStreaming>>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC& rpc) -> asio::awaitable<void>
        {
            RPC::Request request;
            while (co_await rpc.read(request))
            {
                std::cout << "Request: " << request.integer() << std::endl;
            }
            RPC::Response response;
            response.set_integer(42);
            co_await rpc.finish(response, grpc::Status::OK);

            // Alternatively finish with an error:
            co_await rpc.finish_with_error(grpc::Status::CANCELLED);
        },
        asio::detached);
}
/* [server-rpc-client-streaming] */

/* [server-rpc-server-streaming] */
void server_rpc_server_streaming(agrpc::GrpcContext& grpc_context,
                                 example::v1::Example::AsyncService& service)
{
    using RPC = asio::use_awaitable_t<>::as_default_on_t<
        agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestServerStreaming>>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC& rpc, RPC::Request& request) -> asio::awaitable<void>
        {
            RPC::Response response;
            for (int i{}; i != request.integer(); ++i)
            {
                response.set_integer(i);
                if (!co_await rpc.write(response))
                {
                    co_return;
                }
            }
            co_await rpc.finish(grpc::Status::OK);
        },
        asio::detached);
}
/* [server-rpc-server-streaming] */

/* [server-rpc-bidirectional-streaming] */
void server_rpc_bidirectional_streaming(agrpc::GrpcContext& grpc_context,
                                        example::v1::Example::AsyncService& service)
{
    using RPC = asio::use_awaitable_t<>::as_default_on_t<agrpc::ServerRPC<
        &example::v1::Example::AsyncService::RequestBidirectionalStreaming>>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [](RPC& rpc) -> asio::awaitable<void>
        {
            RPC::Request request;
            RPC::Response response;
            while (co_await rpc.read(request))
            {
                response.set_integer(request.integer());
                if (!co_await rpc.write(response))
                {
                    co_return;
                }
            }
            response.set_integer(42);
            co_await rpc.write(response, grpc::WriteOptions{}.set_last_message());
            co_await rpc.finish(grpc::Status::OK);
        },
        asio::detached);
}
/* [server-rpc-bidirectional-streaming] */
// clang-format on

/* [server-rpc-handler-with-arena] */
class ArenaRequestMessageFactory
{
  public:
    template <class Request>
    Request& create()
    {
        return *google::protobuf::Arena::Create<Request>(&arena_);
    }

    // This method is optional and can be omitted
    template <class Request>
    void destroy(Request&) noexcept
    {
    }

  private:
    google::protobuf::Arena arena_;
};

template <class Handler>
class RPCHandlerWithArenaRequestMessageFactory
{
  public:
    explicit RPCHandlerWithArenaRequestMessageFactory(Handler handler) : handler_(std::move(handler)) {}

    // unary and server-streaming rpcs
    template <class ServerRPC, class Request>
    decltype(auto) operator()(ServerRPC&& rpc, Request&& request, ArenaRequestMessageFactory&)
    {
        return handler_(std::forward<ServerRPC>(rpc), std::forward<Request>(request));
    }

    // client-streaming and bidi-streaming rpcs
    template <class ServerRPC>
    decltype(auto) operator()(ServerRPC&& rpc)
    {
        return handler_(std::forward<ServerRPC>(rpc));
    }

    ArenaRequestMessageFactory request_message_factory() { return {}; }

  private:
    Handler handler_;
};

template <class ServerRPC>
void register_rpc_handler(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    agrpc::register_awaitable_rpc_handler<ServerRPC>(
        grpc_context, service,
        RPCHandlerWithArenaRequestMessageFactory{[&](ServerRPC&, typename ServerRPC::Request&) -> asio::awaitable<void>
                                                 {
                                                     // ...
                                                     co_return;
                                                 }},
        asio::detached);
}
/* [server-rpc-handler-with-arena] */
