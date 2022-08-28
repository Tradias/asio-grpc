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

template <class CompletionToken, class Signature, class = void>
struct HandlerType
{
    using Type = char[256];
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

template <class Coroutine, class ReturnType>
struct RebindCoroutine
{
    using Type = Coroutine;
};

template <class T, class Executor, class ReturnType>
struct RebindCoroutine<asio::awaitable<T, Executor>, ReturnType>
{
    using Type = asio::awaitable<void, Executor>;
};

template <class Coroutine, class ReturnType>
using RebindCoroutineT = typename detail::RebindCoroutine<Coroutine, ReturnType>::Type;

template <class Coroutine>
struct CoroutineCompletionToken
{
    using Type = Coroutine;
};

template <class T, class Executor>
struct CoroutineCompletionToken<asio::awaitable<T, Executor>>
{
    using Type = asio::use_awaitable_t<Executor>;
};

template <class Coroutine>
using CoroutineCompletionTokenT = typename detail::CoroutineCompletionToken<Coroutine>::Type;

template <class Coroutine>
inline constexpr CoroutineCompletionTokenT<Coroutine> USE_COROUTINE{};

template <class Coroutine>
using CoroutineExecutorT = typename Coroutine::executor_type;

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

template <std::size_t Size>
class GuardedBuffer
{
  public:
    GuardedBuffer() = default;
    GuardedBuffer(const GuardedBuffer& other) = delete;
    GuardedBuffer(GuardedBuffer&& other) = delete;
    GuardedBuffer& operator=(const GuardedBuffer& other) = delete;
    GuardedBuffer& operator=(GuardedBuffer&& other) = delete;

    ~GuardedBuffer() noexcept
    {
        if (destructor)
        {
            destructor(&buffer_);
        }
    }

    template <class T>
    void assign(T&& t)
    {
        using Tunref = detail::RemoveCrefT<T>;
        detail::construct_at(reinterpret_cast<Tunref*>(&buffer_), static_cast<T&&>(t));
        destructor = &detail::destruct<Tunref>;
    }

    void release() noexcept { destructor = nullptr; }

    void* get() noexcept { return buffer_; }

  private:
    using Destructor = void (*)(void*);

    alignas(std::max_align_t) std::byte buffer_[Size];
    Destructor destructor{};
};

template <class Coroutine>
class CoroutineSubPool : public detail::CoroutineSubPoolBase
{
  private:
    static constexpr std::size_t MAX_COROUTINES = 250;

    using Executor = detail::CoroutineExecutorT<Coroutine>;
    using UseCoroutine = const detail::CoroutineCompletionTokenT<Coroutine>;
    using Operation = detail::TypeErasedCoroutinePoolOperation<Coroutine>;
    using ExecuteFunction = void (*)(void*, Operation*);
    using CoroutineSignature = void(Operation*);
    using Buffer = detail::GuardedBuffer<sizeof(detail::CompletionHandlerTypeT<UseCoroutine, CoroutineSignature>)>;

    struct CoroutineContext : detail::IntrusiveQueueHook<CoroutineContext>
    {
        void execute(Operation* operation) { execute_(completion_handler->get(), operation); }

        Buffer* completion_handler;
        ExecuteFunction execute_;
    };

  public:
    explicit CoroutineSubPool(const Executor& executor) noexcept : executor(executor) {}

    ~CoroutineSubPool() noexcept
    {
        // auto& grpc_context = detail::query_grpc_context(executor);
        // is_stopped = true;
        // while (!coroutine_contexts.empty())
        // {
        //     CoroutineContext* context = coroutine_contexts.pop_front();
        //     context->execute(
        //         const_cast<NoOpCoroutinePoolOperation<Coroutine>*>(&NO_OP_COROUTINE_POOL_OPERATION<Coroutine>));
        // }
    }

    void execute(Operation* operation)
    {
        if (coroutine_contexts.empty())
        {
            if (coroutine_count < MAX_COROUTINES)
            {
                ++coroutine_count;
                asio::co_spawn(executor, this->coroutine_function(operation), detail::RethrowFirstArg{});
            }
            else
            {
                asio::co_spawn(executor, operation->complete(), detail::RethrowFirstArg{});
            }
        }
        else
        {
            CoroutineContext* context = coroutine_contexts.pop_front();
            context->execute(operation);
        }
    }

  private:
    template <class CompletionHandler>
    static void invoke_completion_handler(void* completion_handler, Operation* operation)
    {
        auto ch = static_cast<CompletionHandler&&>(*static_cast<CompletionHandler*>(completion_handler));
        static_cast<CompletionHandler&&>(ch)(operation);
    }

    static auto initiate_wait(Buffer& buffer, CoroutineContext& context)
    {
        return asio::async_initiate<UseCoroutine, CoroutineSignature>(
            [&](auto&& completion_handler)
            {
                using CH = detail::RemoveCrefT<decltype(completion_handler)>;
                static_assert(alignof(std::max_align_t) >= alignof(CH),
                              "Overaligned completion handlers are not supported");
                buffer.assign(static_cast<CH&&>(completion_handler));
                context.execute_ = &CoroutineSubPool::invoke_completion_handler<CH>;
            },
            detail::USE_COROUTINE<Coroutine>);
    }

    Coroutine coroutine_function()
    {
        Buffer completion_handler_buffer;
        CoroutineContext context;
        context.completion_handler = &completion_handler_buffer;
        while (true)
        {
            coroutine_contexts.push_back(&context);
            auto& grpc_context = detail::query_grpc_context(executor);
            detail::FinishWorkAndGuard on_exit{grpc_context};
            auto* operation = co_await CoroutineSubPool::initiate_wait(completion_handler_buffer, context);
            completion_handler_buffer.release();
            on_exit.fire();
            co_await operation->complete();
        }
    }

    Coroutine coroutine_function(Operation* operation)
    {
        co_await operation->complete();
        auto& grpc_context = detail::query_grpc_context(executor);
        detail::FinishWorkAndGuard on_exit{grpc_context};
        co_await this->coroutine_function();
    }

    Executor executor;
    detail::IntrusiveQueue<CoroutineContext> coroutine_contexts;
    std::size_t coroutine_count{};
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
