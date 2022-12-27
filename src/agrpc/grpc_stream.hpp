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

#ifndef AGRPC_AGRPC_GRPC_STREAM_HPP
#define AGRPC_AGRPC_GRPC_STREAM_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include <agrpc/bind_allocator.hpp>
#include <agrpc/cancel_safe.hpp>
#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/async_initiate.hpp>
#include <agrpc/detail/get_completion_queue.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Cancellation safety for streaming RPCs
 *
 * Lightweight, IoObject-like class with cancellation safety for RPC functions.
 *
 * @since 1.7.0 (and Boost.Asio 1.77.0)
 */
template <class Executor>
class BasicGrpcStream
{
  private:
    class CompletionHandler;

  public:
    /**
     * @brief The associated executor type
     */
    using executor_type = Executor;

    /**
     * @brief Rebind the stream to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The stream type when rebound to the specified executor
         */
        using other = BasicGrpcStream<OtherExecutor>;
    };

    /**
     * @brief Construct from an executor
     */
    template <class Exec>
    explicit BasicGrpcStream(Exec&& executor) noexcept : executor(static_cast<Exec&&>(executor))
    {
    }

    /**
     * @brief Construct from a `agrpc::GrpcContext`
     */
    explicit BasicGrpcStream(agrpc::GrpcContext& grpc_context) noexcept : executor(grpc_context.get_executor()) {}

    /**
     * @brief Get the associated executor
     *
     * Thread-safe
     */
    [[nodiscard]] const executor_type& get_executor() const noexcept { return this->executor; }

    /**
     * @brief Is an operation currently running?
     *
     * Thread-safe
     */
    [[nodiscard]] bool is_running() const noexcept { return this->running.load(std::memory_order_relaxed); }

    /**
     * @brief Wait for the initiated operation to complete
     *
     * Only one call to `next()` may be outstanding at a time.
     *
     * **Per-Operation Cancellation**
     *
     * All. Upon cancellation, the initiated operation continues to run.
     */
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto next(CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return this->safe.wait(static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Initiate an operation using the specified allocator
     *
     * Only one operation may be running at a time.
     */
    template <class Allocator, class Function, class... Args>
    BasicGrpcStream& initiate(std::allocator_arg_t, Allocator allocator, Function&& function, Args&&... args)
    {
        this->running.store(true, std::memory_order_relaxed);
        std::invoke(static_cast<Function&&>(function), static_cast<Args&&>(args)...,
                    agrpc::bind_allocator(allocator, CompletionHandler{*this}));
        return *this;
    }

    /**
     * @brief Initiate an operation
     *
     * Only one operation may be running at a time.
     */
    template <class Function, class... Args>
    BasicGrpcStream& initiate(Function&& function, Args&&... args)
    {
        this->running.store(true, std::memory_order_relaxed);
        std::invoke(static_cast<Function&&>(function), static_cast<Args&&>(args)..., CompletionHandler{*this});
        return *this;
    }

    /**
     * @brief Either wait for the initiated operation to complete or complete immediately
     *
     * If the initiated operation has already completed then the completion handler will be invoked in a manner
     * equivalent to using `asio::post`.
     *
     * **Per-Operation Cancellation**
     *
     * All. Upon cancellation, the initiated operation continues to run.
     */
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto cleanup(CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        if (this->is_running())
        {
            return this->safe.wait(static_cast<CompletionToken&&>(token));
        }
        return detail::async_initiate_immediate_completion<void(detail::ErrorCode, bool)>(
            static_cast<CompletionToken&&>(token));
    }

  private:
    class CompletionHandler
    {
      public:
        using executor_type = Executor;

        explicit CompletionHandler(BasicGrpcStream& stream) : self(stream) {}

        void operator()(bool ok)
        {
            this->self.running.store(false, std::memory_order_relaxed);
            this->self.safe.token()(ok);
        }

        [[nodiscard]] const executor_type& get_executor() const noexcept { return self.executor; }

      private:
        BasicGrpcStream& self;
    };

    Executor executor;
    agrpc::GrpcCancelSafe safe;
    std::atomic_bool running{};
};

/**
 * @brief (experimental) A BasicGrpcStream that uses `agrpc::DefaultCompletionToken`
 *
 * @since 1.7.0 (and Boost.Asio 1.77.0)
 */
using GrpcStream = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicGrpcStream<agrpc::GrpcExecutor>>;

// Implementation details
namespace detail
{
template <class Executor>
grpc::CompletionQueue* get_completion_queue(const agrpc::BasicGrpcStream<Executor>& grpc_stream) noexcept
{
    return detail::get_completion_queue(grpc_stream.get_executor());
}
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_GRPC_STREAM_HPP
