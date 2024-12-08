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

#ifndef AGRPC_UTILS_ASIO_UTILS_HPP
#define AGRPC_UTILS_ASIO_UTILS_HPP

#include "utils/asio_forward.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/waiter.hpp>

#include <functional>
#include <type_traits>

namespace test
{
struct InvocableArchetype
{
    template <class... Args>
    void operator()(Args&&...) const
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

struct RethrowFirstArg
{
    void operator()(const std::exception_ptr& ep)
    {
        if (ep)
        {
            std::rethrow_exception(ep);
        }
    }
};

template <class Derived>
struct ReceiverBase
{
    using is_receiver = void;

#ifdef AGRPC_STDEXEC
    friend constexpr void tag_invoke(stdexec::set_stopped_t, const ReceiverBase& r) noexcept
    {
        static_cast<const Derived&>(r).set_done();
    }

    template <class... T>
    friend constexpr void tag_invoke(stdexec::set_value_t, const ReceiverBase& r, T&&... args) noexcept
    {
        static_cast<const Derived&>(r).set_value(std::forward<T>(args)...);
    }

    friend void tag_invoke(stdexec::set_error_t, const ReceiverBase& r, std::exception_ptr e) noexcept
    {
        static_cast<const Derived&>(r).set_error(static_cast<std::exception_ptr&&>(e));
    }
#endif
};

template <class Function, class Allocator = std::allocator<void>, class Derived = void>
struct FunctionAsReceiver
    : ReceiverBase<std::conditional_t<std::is_same_v<void, Derived>, FunctionAsReceiver<Function, Allocator>, Derived>>
{
    using allocator_type = Allocator;

    Function function;
    Allocator allocator;

    explicit FunctionAsReceiver(Function function, const Allocator& allocator = {})
        : function(std::move(function)), allocator(allocator)
    {
    }

    static void set_done() noexcept {}

    template <class... Args>
    void set_value(Args&&... args) const noexcept
    {
        function(std::forward<Args>(args)...);
    }

    static void set_error(std::exception_ptr) noexcept {}

    auto get_allocator() const noexcept { return allocator; }

#ifdef AGRPC_UNIFEX
    friend Allocator tag_invoke(unifex::tag_t<unifex::get_allocator>, const FunctionAsReceiver& receiver) noexcept
    {
        return receiver.allocator;
    }
#endif
};

struct StatefulReceiverState
{
    std::exception_ptr exception{};
    bool was_done{false};
};

template <class Function, class Allocator = std::allocator<void>>
struct FunctionAsStatefulReceiver
    : test::FunctionAsReceiver<Function, Allocator, FunctionAsStatefulReceiver<Function, Allocator>>
{
    test::StatefulReceiverState& state;

    FunctionAsStatefulReceiver(Function function, StatefulReceiverState& state, const Allocator& allocator = {})
        : test::FunctionAsReceiver<Function, Allocator, FunctionAsStatefulReceiver>(std::move(function), allocator),
          state(state)
    {
    }

    void set_done() const noexcept { state.was_done = true; }

    void set_error(const std::exception_ptr& ptr) const noexcept { state.exception = ptr; }
};

template <bool IsNothrow>
struct ConditionallyNoexceptNoOpReceiver : ReceiverBase<ConditionallyNoexceptNoOpReceiver<IsNothrow>>
{
    ConditionallyNoexceptNoOpReceiver() noexcept(IsNothrow) {}

    ConditionallyNoexceptNoOpReceiver(const ConditionallyNoexceptNoOpReceiver&) noexcept(IsNothrow) {}

    ConditionallyNoexceptNoOpReceiver(ConditionallyNoexceptNoOpReceiver&&) noexcept(IsNothrow) {}

    ConditionallyNoexceptNoOpReceiver& operator=(const ConditionallyNoexceptNoOpReceiver&) noexcept(IsNothrow)
    {
        return *this;
    }

    ConditionallyNoexceptNoOpReceiver& operator=(ConditionallyNoexceptNoOpReceiver&&) noexcept(IsNothrow)
    {
        return *this;
    }

    static void set_done() noexcept {}

    template <class... Args>
    static void set_value(Args&&...) noexcept
    {
    }

    static void set_error(std::exception_ptr) noexcept {}
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Handler, class = void>
struct HandlerWithAssociatedAllocatorExecutor
{
    Handler handler;
};

template <class Handler>
struct HandlerWithAssociatedAllocatorExecutor<Handler, std::void_t<typename Handler::executor_type>>
{
    Handler handler;

    using executor_type = typename Handler::executor_type;

    [[nodiscard]] decltype(auto) get_executor() const noexcept { return asio::get_associated_executor(handler); }
};

template <class Handler, class Allocator>
struct HandlerWithAssociatedAllocator : HandlerWithAssociatedAllocatorExecutor<Handler>
{
    using allocator_type = Allocator;

    Allocator allocator;

    HandlerWithAssociatedAllocator(Handler handler, const Allocator& allocator)
        : HandlerWithAssociatedAllocatorExecutor<Handler>{std::move(handler)}, allocator(allocator)
    {
    }

    template <class... Args>
    decltype(auto) operator()(Args&&... args)
    {
        return this->handler(std::forward<Args>(args)...);
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator; }
};

void wait(agrpc::Alarm& alarm, std::chrono::system_clock::time_point deadline,
          const std::function<void(bool)>& function);

void spawn(agrpc::GrpcContext& grpc_context, const std::function<void(const asio::yield_context&)>& function);

void spawn(asio::io_context& io_context, const std::function<void(const asio::yield_context&)>& function);

template <class Executor, class Function>
void typed_spawn(Executor&& executor, Function&& function)
{
#ifdef AGRPC_TEST_ASIO_HAS_NEW_SPAWN
    asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function), test::RethrowFirstArg{});
#else
    asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function));
#endif
}

template <class Executor, class... Functions>
void spawn_many(Executor&& executor, Functions&&... functions)
{
    (test::spawn(executor, std::forward<Functions>(functions)), ...);
}

template <class... Functions>
void spawn_and_run(agrpc::GrpcContext& grpc_context, Functions&&... functions)
{
    (test::spawn(grpc_context, std::forward<Functions>(functions)), ...);
    grpc_context.run();
}

template <class CompletionToken, class Signature, class Initiation, class RawCompletionToken, class... Args>
decltype(auto) initiate_using_async_completion(Initiation&& initiation, RawCompletionToken&& token, Args&&... args)
{
    asio::async_completion<CompletionToken, Signature> completion(token);
    std::forward<Initiation>(initiation)(std::move(completion.completion_handler), std::forward<Args>(args)...);
    return completion.result.get();
}

void post(agrpc::GrpcContext& grpc_context, const std::function<void()>& function);

void post(const agrpc::GrpcExecutor& executor, const std::function<void()>& function);

template <class Signature, class Executor>
void complete_immediately(agrpc::GrpcContext& grpc_context, agrpc::Waiter<Signature, Executor>& waiter)
{
    waiter.initiate(
        [&](auto&& context, auto&& token)
        {
            asio::post(context, token);
        },
        grpc_context);
}

#ifdef AGRPC_TEST_ASIO_HAS_CO_AWAIT
void co_spawn(agrpc::GrpcContext& grpc_context, const std::function<asio::awaitable<void>()>& function);

template <class Executor>
void co_spawn(Executor&& executor, const std::function<asio::awaitable<void>()>& function)
{
    asio::co_spawn(std::forward<Executor>(executor), function, test::RethrowFirstArg{});
}

template <class Executor, class Function>
void co_spawn(Executor&& executor, Function function)
{
    using Erased = std::function<asio::awaitable<void>()>;
    if constexpr (std::is_constructible_v<Erased, Function>)
    {
        test::co_spawn(std::forward<Executor>(executor), Erased(function));
    }
    else
    {
        asio::co_spawn(std::forward<Executor>(executor), std::move(function), test::RethrowFirstArg{});
    }
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
