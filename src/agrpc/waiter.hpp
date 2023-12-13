// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_AGRPC_WAITER_HPP
#define AGRPC_AGRPC_WAITER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/waiter.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Utility class for uncancelable operations
 *
 * Provides cancellation support for otherwise uncancelable operations by canceling only the act of waiting for
 * completion as opposed to the operation itself.
 *
 * @tparam Signature Completion signature of the operation. For example, for `agrpc::Alarm::wait` this would be
 * `void(bool)`.
 * @tparam Executor Type of the I/O executor. For `agrpc::Alarm` this would be `agrpc::GrpcExecutor` or
 * `asio::any_io_executor`. Default: `agrpc::GrpcExecutor`
 *
 * @since 2.7.0 (and Boost.Asio 1.77.0)
 */
template <class Signature, class Executor>
class Waiter
{
  public:
    /**
     * @brief The associated executor type
     */
    using executor_type = Executor;

    /**
     * @brief Rebind the Waiter to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The Waiter type when rebound to the specified executor
         */
        using other = Waiter<Signature, OtherExecutor>;
    };

    /**
     * @brief Default construct a Waiter
     */
    Waiter() noexcept {}

    Waiter(const Waiter& other) = delete;
    Waiter(Waiter&& other) = delete;

    /**
     * @brief Destruct the Waiter
     *
     * All initiated operations have been `wait()`ed for, otherwise the behavior is undefined.
     */
    ~Waiter() noexcept { destroy_executor(); }

    Waiter& operator=(const Waiter& other) = delete;
    Waiter& operator=(Waiter&& other) = delete;

    /**
     * @brief Initiate an operation
     *
     * Only one operation may be running at a time.
     *
     * All operations must complete before this object is destructed.
     *
     * Example:
     *
     * @snippet server_rpc.cpp waiter-example
     *
     * @param function Callable that will be invoked with all subsequent arguments followed by the completion handler of
     * this Waiter (Asio) or it returns a sender composed of the result of the call (unifex/stdexec only).
     * @param executor_or_io_object Either an executor itself or an object that implements `get_executor()`. This will
     * become the I/O executor of subsequent calls to `wait()`.
     */
    template <class Function, class ExecutorOrIoObject, class... Args>
    decltype(auto) initiate(Function&& function, ExecutorOrIoObject&& executor_or_io_object, Args&&... args)
    {
        destroy_executor();
        ::new (static_cast<void*>(&executor_)) Executor(detail::get_executor_from_io_object(executor_or_io_object));
        event_.reset();
        detail::WaiterCompletionHandler<Signature> token{event_};
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
        return std::invoke(static_cast<Function&&>(function), static_cast<ExecutorOrIoObject&&>(executor_or_io_object),
                           static_cast<Args&&>(args)..., token);
#else
        return detail::exec::then(
            std::invoke(static_cast<Function&&>(function), static_cast<ExecutorOrIoObject&&>(executor_or_io_object),
                        static_cast<Args&&>(args)...),
            token);
#endif
    }

    /**
     * @brief Has the initiated operation finished?
     *
     * Thread-safe
     */
    [[nodiscard]] bool is_ready() const noexcept { return event_.ready(); }

    /**
     * @brief Wait for the initiated operation to complete
     *
     * Only one call to `wait()` may be outstanding at a time. May be called before an operation has been initiated.
     * Care must be taken when invoking this function multiple times for an already completed operation as completion
     * arguments are moved into the completion handler.
     *
     * **Per-Operation Cancellation**
     *
     * All. Upon cancellation, the initiated operation continues to run.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait(CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return wait_impl(static_cast<CompletionToken&&>(token));
    }

  private:
    template <class>
    friend class detail::WaiterCompletionHandler;

    template <class CompletionToken>
    auto wait_impl(CompletionToken&& token)
    {
        return event_.wait(static_cast<CompletionToken&&>(token), executor_);
    }

    auto wait_impl(agrpc::UseSender) noexcept { return event_.wait(); }

    void destroy_executor() noexcept
    {
        if (event_.ready())
        {
            executor_.~Executor();
        }
    }

    detail::ManualResetEvent<Signature> event_{};
    union
    {
        Executor executor_;
    };
};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_WAITER_HPP
