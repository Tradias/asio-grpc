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

#ifndef AGRPC_UTILS_ASIOUTILS_HPP
#define AGRPC_UTILS_ASIOUTILS_HPP

#include "utils/asioForward.hpp"

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

template <class Function, class Allocator = std::allocator<std::byte>>
struct FunctionAsReceiver
{
    using allocator_type = Allocator;

    Function function;
    std::exception_ptr exception;
    bool was_done{false};
    Allocator allocator;

    explicit FunctionAsReceiver(Function function, Allocator allocator = {})
        : function(std::move(function)), allocator(allocator)
    {
    }

    void set_done() noexcept { was_done = true; }

    template <class... Args>
    void set_value(Args&&... args)
    {
        function(std::forward<Args>(args)...);
    }

    void set_error(std::exception_ptr ptr) noexcept { exception = ptr; }

    auto get_allocator() const noexcept { return allocator; }

#ifdef AGRPC_UNIFEX
    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const FunctionAsReceiver& receiver) noexcept
        -> Allocator
    {
        return receiver.allocator;
    }
#endif
};

template <class SenderFactory, class Allocator = std::allocator<void>>
struct Submitter
{
    agrpc::GrpcContext& grpc_context;
    SenderFactory sender_factory;
    Allocator allocator;

    Submitter(agrpc::GrpcContext& grpc_context, SenderFactory sender_factory, Allocator allocator = {})
        : grpc_context(grpc_context), sender_factory(sender_factory), allocator(allocator)
    {
    }

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& context, bool ok)
    {
        if (!ok)
        {
            return;
        }
        auto args = context.args();
        agrpc::detail::submit(std::apply(sender_factory, args),
                              test::FunctionAsReceiver{[c = std::move(context)](auto&&...) {}});
    }

#ifdef AGRPC_UNIFEX
    friend auto tag_invoke(unifex::tag_t<unifex::get_scheduler>, const Submitter& self) noexcept
    {
        return self.grpc_context.get_scheduler();
    }

    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const Submitter& self) noexcept
    {
        return self.allocator;
    }
#endif
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Handler, class Allocator>
struct HandlerWithAssociatedAllocator
{
    using executor_type = asio::associated_executor_t<Handler>;
    using allocator_type = Allocator;

    Handler handler;
    Allocator allocator;

    HandlerWithAssociatedAllocator(Handler handler, Allocator allocator)
        : handler(std::move(handler)), allocator(allocator)
    {
    }

    decltype(auto) operator()() { return handler(); }

    [[nodiscard]] executor_type get_executor() const noexcept { return asio::get_associated_executor(handler); }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator; }
};

template <class Executor, class Handler, class Allocator>
struct RpcSpawner
{
    using executor_type = Executor;
    using allocator_type = Allocator;

    Executor executor;
    Handler handler;
    Allocator allocator;

    RpcSpawner(Executor executor, Handler handler, Allocator allocator)
        : executor(std::move(executor)), handler(std::move(handler)), allocator(allocator)
    {
    }

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& context)
    {
        asio::spawn(get_executor(),
                    [handler = std::move(handler), context = std::move(context)](auto&& yield_context) mutable
                    {
                        std::apply(std::move(handler),
                                   std::tuple_cat(context.args(), std::forward_as_tuple(std::move(yield_context))));
                    });
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator; }
};

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
#endif
#endif
}  // namespace test

#endif  // AGRPC_UTILS_ASIOUTILS_HPP
