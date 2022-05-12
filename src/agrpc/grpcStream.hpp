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

#ifndef AGRPC_AGRPC_GRPCSTREAM_HPP
#define AGRPC_AGRPC_GRPCSTREAM_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include <agrpc/bindAllocator.hpp>
#include <agrpc/cancelSafe.hpp>
#include <agrpc/defaultCompletionToken.hpp>
#include <agrpc/detail/asyncInitiate.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpcContext.hpp>
#include <agrpc/grpcExecutor.hpp>

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
    template <class Allocator>
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
    explicit BasicGrpcStream(Exec&& executor) noexcept : executor(std::forward<Exec>(executor))
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
    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

    /**
     * @brief Is an operation currently running?
     *
     * Thread-safe
     */
    [[nodiscard]] bool is_running() const noexcept { return running.load(std::memory_order_relaxed); }

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
        return safe.wait(std::forward<CompletionToken>(token));
    }

    /**
     * @brief Initiate an operation using the specified allocator
     *
     * Only one operation may be running at a time.
     */
    template <class Allocator, class Function, class... Args>
    auto& initiate(std::allocator_arg_t, Allocator allocator, Function&& function, Args&&... args)
    {
        running.store(true, std::memory_order_relaxed);
        std::forward<Function>(function)(std::forward<Args>(args)..., CompletionHandler<Allocator>{*this, allocator});
        return *this;
    }

    /**
     * @brief Initiate an operation
     *
     * Only one operation may be running at a time.
     */
    template <class Function, class... Args>
    auto& initiate(Function&& function, Args&&... args)
    {
        this->initiate(std::allocator_arg, std::allocator<void>{}, std::forward<Function>(function),
                       std::forward<Args>(args)...);
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
        if (!this->is_running())
        {
            return detail::async_initiate_immediate_completion<void(detail::ErrorCode, bool)>(
                std::forward<CompletionToken>(token));
        }
        return safe.wait(std::forward<CompletionToken>(token));
    }

  private:
    template <class Allocator>
    class CompletionHandler
    {
      public:
        using executor_type = Executor;
        using allocator_type = Allocator;

        explicit CompletionHandler(BasicGrpcStream& stream, Allocator allocator) : impl(stream, allocator) {}

        void operator()(bool ok)
        {
            auto& self = impl.first();
            self.running.store(false, std::memory_order_relaxed);
            self.safe.token()(ok);
        }

        [[nodiscard]] executor_type get_executor() const noexcept { return impl.first().executor; }

        [[nodiscard]] allocator_type get_allocator() const noexcept { return impl.second(); }

      private:
        detail::CompressedPair<BasicGrpcStream&, Allocator> impl;
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

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_GRPCSTREAM_HPP
