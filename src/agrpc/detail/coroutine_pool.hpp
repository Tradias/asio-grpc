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

#ifndef AGRPC_DETAIL_COROUTINE_POOL_HPP
#define AGRPC_DETAIL_COROUTINE_POOL_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/intrusive_queue.hpp>
#include <agrpc/detail/intrusive_queue_hook.hpp>
#include <agrpc/detail/memory.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/type_erased_operation.hpp>
#include <agrpc/detail/utility.hpp>

#include <memory>
#include <vector>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RethrowFirstArg
{
    template <class... Args>
    void operator()(std::exception_ptr ep, Args&&...) const
    {
        if AGRPC_UNLIKELY (ep)
        {
            std::rethrow_exception(ep);
        }
    }
};

struct CompletionHandlerUnknown
{
    char d[256];
};

template <class CompletionToken, class Signature, class = void>
struct HandlerType
{
    using Type = CompletionHandlerUnknown;
};

template <class CompletionToken, class Signature>
struct HandlerType<CompletionToken, Signature,
                   std::void_t<typename asio::async_result<CompletionToken, Signature>::handler_type>>
{
    using Type = typename asio::async_result<CompletionToken, Signature>::handler_type;
};

template <class CompletionToken, class Signature, class = void>
struct CompletionHandlerType
{
    using Type = typename detail::HandlerType<CompletionToken, Signature>::Type;
};

template <class CompletionToken, class Signature>
struct CompletionHandlerType<
    CompletionToken, Signature,
    std::void_t<typename asio::async_result<CompletionToken, Signature>::completion_handler_type>>
{
    using Type = typename asio::async_result<CompletionToken, Signature>::completion_handler_type;
};

template <class CompletionToken, class Signature>
using CompletionHandlerTypeT = typename CompletionHandlerType<CompletionToken, Signature>::Type;

struct CoroutineTypeTag
{
};

template <class Coroutine>
inline constexpr CoroutineTypeTag COROUTINE_TYPE_TAG{};

template <class Coroutine>
struct CoroutineTraits;

template <class T, class Executor>
struct CoroutineTraits<asio::awaitable<T, Executor>>
{
    using ExecutorType = Executor;
    using CompletionToken = asio::use_awaitable_t<Executor>;

    template <class U>
    using Rebind = asio::awaitable<U, Executor>;
};

template <class Coroutine, class ReturnType>
using RebindCoroutineT = typename detail::CoroutineTraits<Coroutine>::template Rebind<ReturnType>;

template <class Coroutine>
using CoroutineCompletionTokenT = typename detail::CoroutineTraits<Coroutine>::CompletionToken;

template <class Coroutine>
inline constexpr CoroutineCompletionTokenT<Coroutine> USE_COROUTINE{};

template <class Coroutine>
using CoroutineExecutorT = typename detail::CoroutineTraits<Coroutine>::ExecutorType;

template <class Coroutine>
class CoroutineSubPool;

class CoroutineSubPoolBase
{
};

using CoroutineSubPoolDeleter = void (*)(CoroutineSubPoolBase*);

using CoroutineSubPoolPtr = std::unique_ptr<CoroutineSubPoolBase, CoroutineSubPoolDeleter>;

template <class Coroutine>
CoroutineSubPoolPtr create_coroutine_sub_pool(const detail::CoroutineExecutorT<Coroutine>& executor);

class CoroutinePool
{
  private:
    struct TaggedSubPool
    {
        const CoroutineTypeTag* tag;
        CoroutineSubPoolPtr sub_pool;
    };

  public:
    template <class Coroutine>
    auto& get_or_create_sub_pool(const detail::CoroutineExecutorT<Coroutine>& executor)
    {
        static_assert(std::is_same_v<detail::RebindCoroutineT<Coroutine, void>, Coroutine>,
                      "Programming error: Coroutine type must return void");
        const auto found = std::find_if(sub_pools.begin(), sub_pools.end(),
                                        [](const TaggedSubPool& sub_pool)
                                        {
                                            return sub_pool.tag == &COROUTINE_TYPE_TAG<Coroutine>;
                                        });
        if (found == sub_pools.end())
        {
            sub_pools.push_back(
                {&COROUTINE_TYPE_TAG<Coroutine>, detail::create_coroutine_sub_pool<Coroutine>(executor)});
            return *static_cast<CoroutineSubPool<Coroutine>*>(sub_pools.back().sub_pool.get());
        }
        return *static_cast<CoroutineSubPool<Coroutine>*>(found->sub_pool.get());
    }

  private:
    std::vector<TaggedSubPool> sub_pools;
};

template <class Coroutine>
class TypeErasedCoroutinePoolOperation
{
  public:
    using OnComplete = Coroutine (*)(TypeErasedCoroutinePoolOperation*);

    auto complete() { return this->on_complete(this); }

  protected:
    constexpr explicit TypeErasedCoroutinePoolOperation(OnComplete on_complete) noexcept : on_complete(on_complete) {}

  private:
    OnComplete on_complete;
};

template <class Coroutine>
struct NoOpCoroutinePoolOperation : TypeErasedCoroutinePoolOperation<Coroutine>
{
    constexpr NoOpCoroutinePoolOperation() : TypeErasedCoroutinePoolOperation<Coroutine>(&do_complete) {}

    static Coroutine do_complete(TypeErasedCoroutinePoolOperation<Coroutine>*) { co_return; }
};

template <class Coroutine>
inline constexpr NoOpCoroutinePoolOperation<Coroutine> NO_OP_COROUTINE_POOL_OPERATION{};

template <class Coroutine>
class CoroutineSubPool : public detail::CoroutineSubPoolBase
{
  private:
    // Make sure to adjust the test when changing this value
    static constexpr std::size_t MAX_COROUTINES = 250;

    using CoroTraits = detail::CoroutineTraits<Coroutine>;
    using Executor = typename CoroTraits::ExecutorType;
    using UseCoroutine = const typename CoroTraits::CompletionToken;
    using Operation = detail::TypeErasedCoroutinePoolOperation<Coroutine>;
    using ExecuteFunction = void (*)(void*, Operation*);
    using CoroutineSignature = void(Operation*);
    using CompletionHandler = detail::CompletionHandlerTypeT<UseCoroutine, CoroutineSignature>;
    using CompletionHandlerBuffer =
        detail::ConditionalT<std::is_same_v<detail::CompletionHandlerUnknown, CompletionHandler>, detail::DelayedBuffer,
                             detail::StackBuffer<sizeof(CompletionHandler)>>;

    struct CoroutineContext : detail::IntrusiveQueueHook<CoroutineContext>
    {
        void execute(Operation* operation) { execute_(completion_handler, operation); }

        void* completion_handler;
        ExecuteFunction execute_;
    };

  public:
    using executor_type = Executor;

    explicit CoroutineSubPool(const Executor& executor) noexcept : executor(executor) {}

    ~CoroutineSubPool() noexcept
    {
        is_stopped = true;
        while (!coroutine_contexts.empty())
        {
            CoroutineContext* context = coroutine_contexts.pop_front();
            context->execute(
                const_cast<NoOpCoroutinePoolOperation<Coroutine>*>(&NO_OP_COROUTINE_POOL_OPERATION<Coroutine>));
        }
    }

    void execute(Operation* operation)
    {
#ifdef AGRPC_ASIO_HAS_FIXED_AWAITABLES
        auto* context = pop_coroutine_context();
        if (!context)
        {
            if (coroutine_count < MAX_COROUTINES)
            {
                ++coroutine_count;
                asio::co_spawn(executor, this->coroutine_function(operation), detail::RethrowFirstArg{});
            }
            else
            {
#endif
                asio::co_spawn(executor, operation->complete(), detail::RethrowFirstArg{});
#ifdef AGRPC_ASIO_HAS_FIXED_AWAITABLES
            }
        }
        else
        {
            context->execute(operation);
        }
#endif
    }

  private:
    template <class CompletionHandler>
    static void invoke_completion_handler(void* completion_handler, Operation* operation)
    {
        auto ch = static_cast<CompletionHandler&&>(*static_cast<CompletionHandler*>(completion_handler));
        static_cast<CompletionHandler&&>(ch)(operation);
    }

    auto initiate_wait(CompletionHandlerBuffer& buffer, CoroutineContext& context)
    {
        return asio::async_initiate<UseCoroutine, CoroutineSignature>(
            [&](auto&& completion_handler)
            {
                using CH = detail::RemoveCrefT<decltype(completion_handler)>;
                buffer.assign(static_cast<CH&&>(completion_handler));
                context.completion_handler = buffer.get();
                context.execute_ = &CoroutineSubPool::invoke_completion_handler<CH>;
                push_coroutine_context(context);
            },
            detail::USE_COROUTINE<Coroutine>);
    }

    Coroutine coroutine_function()
    {
        CompletionHandlerBuffer completion_handler_buffer;
        CoroutineContext context;
        while (!is_stopped.load(std::memory_order_relaxed))
        {
            detail::FinishWorkAndGuard on_exit{get_grpc_context()};
            auto* operation = co_await initiate_wait(completion_handler_buffer, context);
            on_exit.fire();
            co_await operation->complete();
        }
    }

    Coroutine coroutine_function(Operation* operation)
    {
        co_await operation->complete();
        detail::FinishWorkAndGuard on_exit{get_grpc_context()};
        co_await this->coroutine_function();
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

    [[nodiscard]] agrpc::GrpcContext& get_grpc_context() const noexcept { return detail::query_grpc_context(executor); }

    void push_coroutine_context(CoroutineContext& context)
    {
        std::unique_lock lock{contexts_mutex};
        coroutine_contexts.push_back(&context);
    }

    [[nodiscard]] CoroutineContext* pop_coroutine_context()
    {
        std::unique_lock lock{contexts_mutex};
        if (coroutine_contexts.empty())
        {
            return nullptr;
        }
        return coroutine_contexts.pop_front();
    }

    std::atomic_bool is_stopped{};
    std::mutex contexts_mutex{};
    detail::IntrusiveQueue<CoroutineContext> coroutine_contexts;
    std::size_t coroutine_count{};
    Executor executor;
};

template <class Coroutine>
CoroutineSubPoolPtr create_coroutine_sub_pool(const detail::CoroutineExecutorT<Coroutine>& executor)
{
    return CoroutineSubPoolPtr(new CoroutineSubPool<Coroutine>(executor),
                               [](auto* ptr)
                               {
                                   delete static_cast<CoroutineSubPool<Coroutine>*>(ptr);
                               });
}
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_COROUTINE_POOL_HPP
