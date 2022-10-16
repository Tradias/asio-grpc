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

#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/repeatedly_request_context.hpp>

#include <functional>
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

struct RethrowFirstArg
{
    void operator()(std::exception_ptr ep)
    {
        if (ep)
        {
            std::rethrow_exception(ep);
        }
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

template <bool IsNothrow>
struct ConditionallyNoexceptNoOpReceiver
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

    void set_done() noexcept {}

    template <class... Args>
    void set_value(Args&&...) noexcept
    {
    }

    void set_error(std::exception_ptr) noexcept {}
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

void spawn(agrpc::GrpcContext& grpc_context, const std::function<void(const asio::yield_context&)>& function);

template <class Executor, class Function>
void typed_spawn(Executor&& executor, Function&& function)
{
#ifdef AGRPC_TEST_ASIO_HAS_NEW_SPAWN
    asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function), test::RethrowFirstArg{});
#else
    asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function));
#endif
}

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
        test::typed_spawn(grpc_context,
                          [h = handler, context = std::move(context)](const asio::yield_context& yield_context) mutable
                          {
                              std::apply(std::move(h),
                                         std::tuple_cat(context.args(), std::forward_as_tuple(yield_context)));
                          });
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return grpc_context.get_executor(); }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator; }
};

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

void wait(grpc::Alarm& alarm, std::chrono::system_clock::time_point deadline,
          const asio::executor_binder<std::function<void(bool)>, agrpc::GrpcExecutor>& function);

void post(agrpc::GrpcContext& grpc_context, const std::function<void()>& function);

void post(const agrpc::GrpcExecutor& executor, const std::function<void()>& function);

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
template <class Executor, class CancellationCondition, class CompletionToken, class... Function>
auto parallel_group_bind_executor(const Executor& executor, CancellationCondition cancellation_condition,
                                  CompletionToken&& token, Function&&... function)
{
    return asio::experimental::make_parallel_group(
               [&](auto& f)
               {
                   return [&](auto&& t)
                   {
                       return f(asio::bind_executor(executor, std::move(t)));
                   };
               }(function)...)
        .async_wait(cancellation_condition, std::forward<CompletionToken>(token));
}
#endif

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
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

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
template <class Function, class Allocator>
struct agrpc::asio::traits::set_done_member<test::FunctionAsReceiver<Function, Allocator>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};

template <class Function, class Allocator>
struct agrpc::asio::traits::set_done_member<test::FunctionAsStatefulReceiver<Function, Allocator>>
    : agrpc::asio::traits::set_done_member<test::FunctionAsReceiver<Function, Allocator>>
{
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)
template <class Function, class Allocator, class Vs>
struct agrpc::asio::traits::set_value_member<test::FunctionAsReceiver<Function, Allocator>, Vs>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
};

template <class Function, class Allocator, class Vs>
struct agrpc::asio::traits::set_value_member<test::FunctionAsStatefulReceiver<Function, Allocator>, Vs>
    : agrpc::asio::traits::set_value_member<test::FunctionAsReceiver<Function, Allocator>, Vs>
{
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)
template <class Function, class Allocator, class E>
struct agrpc::asio::traits::set_error_member<test::FunctionAsReceiver<Function, Allocator>, E>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};

template <class Function, class Allocator, class E>
struct agrpc::asio::traits::set_error_member<test::FunctionAsStatefulReceiver<Function, Allocator>, E>
    : agrpc::asio::traits::set_error_member<test::FunctionAsReceiver<Function, Allocator>, E>
{
};
#endif

#endif  // AGRPC_UTILS_ASIO_UTILS_HPP
