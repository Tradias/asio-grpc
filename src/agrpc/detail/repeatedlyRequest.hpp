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

#ifndef AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
#define AGRPC_DETAIL_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/repeatedlyRequestSender.hpp"
#include "agrpc/detail/rpcContext.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/initiate.hpp"
#include "agrpc/rpcs.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Handler, class Args, class = void>
inline constexpr bool IS_REPEATEDLY_REQUEST_SENDER_FACTORY = false;

template <class Handler, class... Args>
inline constexpr bool IS_REPEATEDLY_REQUEST_SENDER_FACTORY<
    Handler, void(Args...), std::enable_if_t<detail::is_sender_v<std::invoke_result_t<Handler&&, Args...>>>> = true;

struct RepeatedlyRequestContextAccess
{
    template <class ImplementationAllocator>
    static constexpr auto create(detail::AllocatedPointer<ImplementationAllocator>&& allocated_pointer) noexcept
    {
        return agrpc::RepeatedlyRequestContext{std::move(allocated_pointer)};
    }
};

template <class RPC, class Service, class Handler>
class RepeatedlyRequestContext
{
  public:
    RepeatedlyRequestContext(RPC rpc, Service& service, Handler handler)
        : impl1(rpc), impl2(service, std::move(handler))
    {
    }

    decltype(auto) rpc() noexcept { return impl1.first(); }

    decltype(auto) stop_context() noexcept { return impl1.second(); }

    decltype(auto) service() noexcept { return impl2.first(); }

    decltype(auto) handler() noexcept { return impl2.second(); }

    auto get_allocator() noexcept { return detail::get_allocator(handler()); }

  private:
    detail::CompressedPair<RPC, detail::Empty> impl1;
    detail::CompressedPair<Service&, Handler> impl2;
};

template <class Operation>
struct RequestRepeater : detail::TypeErasedGrpcTagOperation
{
    using Base = detail::TypeErasedGrpcTagOperation;
    using executor_type = decltype(detail::get_scheduler(std::declval<Operation&>().handler()));
    using RPCContext = detail::RPCContextForRPCT<decltype(std::declval<Operation>().handler().context.rpc())>;

    Operation& operation;
    RPCContext rpc_context;

    explicit RequestRepeater(Operation& operation) : Base(&RequestRepeater::do_complete), operation(operation) {}

    static void do_complete(Base* op, detail::InvokeHandler invoke_handler, bool ok, detail::GrpcContextLocalAllocator);

    decltype(auto) server_context() noexcept { return rpc_context.server_context(); }

    decltype(auto) args() noexcept { return rpc_context.args(); }

    decltype(auto) request() noexcept { return rpc_context.request(); }

    decltype(auto) responder() noexcept { return rpc_context.responder(); }

    auto& context() const noexcept { return operation.handler().context; }

    executor_type get_executor() const noexcept { return detail::get_scheduler(operation.handler()); }

    agrpc::GrpcContext& grpc_context() const noexcept { return detail::query_grpc_context(get_executor()); }

    auto get_allocator() const noexcept { return context().get_allocator(); }

#ifdef AGRPC_UNIFEX
    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const RequestRepeater& self) noexcept
    {
        return detail::get_allocator(self.handler);
    }
#endif
};

template <class Operation>
void complete_repeatedly_request_operation(agrpc::GrpcContext& grpc_context, Operation& operation)
{
    detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
    grpc_context.work_finished();
}

template <class Operation>
auto repeat(Operation& operation)
{
    auto& context = operation.handler().context;
    auto& grpc_context = detail::query_grpc_context(detail::get_scheduler(operation.handler()));
    if (grpc_context.is_stopped())
    {
        return false;
    }
    grpc_context.work_started();
    detail::WorkFinishedOnExit on_exit{grpc_context};
    auto repeater = detail::allocate<detail::RequestRepeater<Operation>>(context.get_allocator(), operation);
    auto args = detail::to_tuple_of_pointers(repeater->rpc_context.args());
    auto* cq = grpc_context.get_server_completion_queue();
    std::apply(context.rpc(), std::tuple_cat(std::forward_as_tuple(context.service()), args,
                                             std::forward_as_tuple(cq, cq, repeater.get())));
    repeater.release();
    on_exit.release();
    return true;
}

template <class Operation>
void RequestRepeater<Operation>::do_complete(Base* op, detail::InvokeHandler invoke_handler, bool ok,
                                             detail::GrpcContextLocalAllocator local_allocator)
{
    auto* self = static_cast<RequestRepeater*>(op);
    detail::AllocatedPointer ptr{self, self->get_allocator()};
    auto& grpc_context = self->grpc_context();
    auto& handler = self->context().handler();
    auto& operation = self->operation;
    if (detail::InvokeHandler::YES == invoke_handler)
    {
        if (ok)
        {
            const auto is_repeated = detail::repeat(operation);
            detail::ScopeGuard guard{[&]
                                     {
                                         if (!is_repeated)
                                         {
                                             detail::complete_repeatedly_request_operation(grpc_context, operation);
                                         }
                                     }};
            handler(detail::RepeatedlyRequestContextAccess::create(std::move(ptr)));
        }
        else
        {
            ptr.reset();
            detail::complete_repeatedly_request_operation(grpc_context, operation);
        }
    }
    else
    {
        ptr.reset();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        operation.complete(invoke_handler, local_allocator);
    }
}

template <class RPC, class Service, class Handler>
struct RepeatedlyRequestInitiator
{
    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler, RPC rpc, Service& service, Handler handler)
    {
        struct CompletionHandlerWithPayload : detail::AssociatedCompletionHandler<CompletionHandler>
        {
            detail::RepeatedlyRequestContext<RPC, Service, Handler> context;

            explicit CompletionHandlerWithPayload(CompletionHandler completion_handler, RPC rpc, Service& service,
                                                  Handler handler)
                : detail::AssociatedCompletionHandler<CompletionHandler>(std::move(completion_handler)),
                  context(rpc, service, std::move(handler))
            {
            }
        };
        const auto executor = detail::get_scheduler(completion_handler);
        const auto allocator = detail::get_allocator(handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        detail::create_no_arg_operation(
            grpc_context, CompletionHandlerWithPayload{std::move(completion_handler), rpc, service, std::move(handler)},
            [&](auto& operation)
            {
                if (!detail::repeat(operation))
                {
                    detail::complete_repeatedly_request_operation(grpc_context, operation);
                }
            },
            allocator);
        on_exit.release();
    }
};

template <class RPC, class Service, class Handler, class CompletionToken>
auto RepeatedlyRequestFn::impl(RPC rpc, Service& service, Handler handler, CompletionToken token)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    using RPCContext = detail::RPCContextForRPCT<RPC>;
    if constexpr (detail::IS_REPEATEDLY_REQUEST_SENDER_FACTORY<Handler, typename RPCContext::Signature>)
    {
#endif
        return detail::RepeatedlyRequestSender{token.grpc_context, rpc, service, std::move(handler)};
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    }
    else
    {
        return asio::async_initiate<CompletionToken, void()>(
            detail::RepeatedlyRequestInitiator<RPC, Service, Handler>{}, token, rpc, service, std::move(handler));
    }
#endif
}

template <class RPC, class Service, class Request, class Responder, class Handler, class CompletionToken>
auto RepeatedlyRequestFn::operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                     Handler handler, CompletionToken token) const
{
    return impl(rpc, service, std::move(handler), std::move(token));
}

template <class RPC, class Service, class Responder, class Handler, class CompletionToken>
auto RepeatedlyRequestFn::operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                     Handler handler, CompletionToken token) const
{
    return impl(rpc, service, std::move(handler), std::move(token));
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
