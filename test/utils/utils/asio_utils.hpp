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

#ifndef AGRPC_UTILS_ASIO_UTILS_HPP
#define AGRPC_UTILS_ASIO_UTILS_HPP

#include "utils/asio_forward.hpp"

#include <agrpc/grpc_executor.hpp>
#include <agrpc/repeatedly_request_context.hpp>

#include <type_traits>

namespace test
{
struct InvocableArchetype
{
    template <class... Args>
    void operator()(Args&&...)
    {
    }
};

struct NoOp
{
    template <class... Args>
    constexpr void operator()(Args&&...) const noexcept
    {
    }
};

template <class Function, class Allocator = std::allocator<void>>
struct FunctionAsReceiver
{
    using allocator_type = Allocator;

    Function function;
    Allocator allocator;

    explicit FunctionAsReceiver(Function function, const Allocator& allocator = {})
        : function(std::move(function)), allocator(allocator)
    {
    }

    void set_done() noexcept {}

    template <class... Args>
    void set_value(Args&&... args)
    {
        function(std::forward<Args>(args)...);
    }

    void set_error(std::exception_ptr) noexcept {}

    auto get_allocator() const noexcept { return allocator; }

#ifdef AGRPC_UNIFEX
    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const FunctionAsReceiver& receiver) noexcept
        -> Allocator
    {
        return receiver.allocator;
    }
#endif
};

template <bool IsNothrow>
struct ConditionallyNoexceptNoOpReceiver
{
    ConditionallyNoexceptNoOpReceiver() = default;

    ConditionallyNoexceptNoOpReceiver(const ConditionallyNoexceptNoOpReceiver&) noexcept(IsNothrow) = default;

    ConditionallyNoexceptNoOpReceiver(ConditionallyNoexceptNoOpReceiver&&) noexcept(IsNothrow) = default;

    ConditionallyNoexceptNoOpReceiver& operator=(const ConditionallyNoexceptNoOpReceiver&) noexcept(IsNothrow) =
        default;

    ConditionallyNoexceptNoOpReceiver& operator=(ConditionallyNoexceptNoOpReceiver&&) noexcept(IsNothrow) = default;

    void set_done() noexcept {}

    template <class... Args>
    void set_value(Args&&... args) noexcept
    {
    }

    void set_error(std::exception_ptr) noexcept {}
};

struct StatefulReceiverState
{
    std::exception_ptr exception{};
    bool was_done{false};
};

template <class Function, class Allocator = std::allocator<void>>
struct FunctionAsStatefulReceiver : public test::FunctionAsReceiver<Function, Allocator>
{
    test::StatefulReceiverState& state;

    FunctionAsStatefulReceiver(Function function, StatefulReceiverState& state, const Allocator& allocator = {})
        : test::FunctionAsReceiver<Function, Allocator>(std::move(function), allocator), state(state)
    {
    }

    void set_done() noexcept { state.was_done = true; }

    void set_error(std::exception_ptr ptr) noexcept { state.exception = ptr; }
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Handler, class Allocator>
struct HandlerWithAssociatedAllocator
{
    using executor_type = asio::associated_executor_t<Handler>;
    using allocator_type = Allocator;

    Handler handler;
    Allocator allocator;

    HandlerWithAssociatedAllocator(Handler handler, const Allocator& allocator)
        : handler(std::move(handler)), allocator(allocator)
    {
    }

    decltype(auto) operator()() { return handler(); }

    [[nodiscard]] executor_type get_executor() const noexcept { return asio::get_associated_executor(handler); }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator; }
};

template <class Handler, class Allocator = std::allocator<void>>
struct RpcSpawner
{
    using executor_type = agrpc::GrpcContext::executor_type;
    using allocator_type = Allocator;

    agrpc::GrpcContext& grpc_context;
    Handler handler;
    Allocator allocator;

    RpcSpawner(agrpc::GrpcContext& grpc_context, Handler handler, const Allocator& allocator = {})
        : grpc_context(grpc_context), handler(std::move(handler)), allocator(allocator)
    {
    }

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& context)
    {
        asio::spawn(get_executor(),
                    [h = handler, context = std::move(context)](auto&& yield_context) mutable
                    {
                        std::apply(std::move(h), std::tuple_cat(context.args(), std::forward_as_tuple(yield_context)));
                    });
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return grpc_context.get_executor(); }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator; }
};

template <class CompletionToken, class Signature, class Initiation, class RawCompletionToken, class... Args>
decltype(auto) initiate_using_async_completion(Initiation&& initiation, RawCompletionToken&& token, Args&&... args)
{
    asio::async_completion<CompletionToken, Signature> completion(token);
    std::forward<Initiation>(initiation)(std::move(completion.completion_handler), std::forward<Args>(args)...);
    return completion.result.get();
}

template <class... Functions>
void spawn_and_run(agrpc::GrpcContext& grpc_context, Functions&&... functions)
{
    (asio::spawn(grpc_context, std::forward<Functions>(functions)), ...);
    grpc_context.run();
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class Executor, class Function>
auto co_spawn(Executor&& executor, Function function)
{
    return asio::co_spawn(std::forward<Executor>(executor), std::move(function),
                          [](std::exception_ptr ep, auto&&...)
                          {
                              if (ep)
                              {
                                  std::rethrow_exception(ep);
                              }
                          });
}

template <class... Functions>
void co_spawn_and_run(agrpc::GrpcContext& grpc_context, Functions&&... functions)
{
    (test::co_spawn(grpc_context, std::forward<Functions>(functions)), ...);
    grpc_context.run();
}
#endif
#endif
}  // namespace test

#endif  // AGRPC_UTILS_ASIO_UTILS_HPP
