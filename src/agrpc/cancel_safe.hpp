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

#ifndef AGRPC_AGRPC_CANCELSAFE_HPP
#define AGRPC_AGRPC_CANCELSAFE_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include <agrpc/detail/cancelSafe.hpp>
#include <agrpc/detail/typeErasedCompletionHandler.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/detail/workTrackingCompletionHandler.hpp>

#include <cassert>
#include <optional>
#include <tuple>

AGRPC_NAMESPACE_BEGIN()

template <class CompletionSignature>
class CancelSafe;

/**
 * @brief Cancellation safety for asynchronous operations
 *
 * This class provides a completion token that can be used to initiate asynchronous operations in a cancellation safe
 * manner. A second method of this class is then used to wait for the operation to complete. Cancelling said waiting
 * will not cancel the underlying operation but still invoke the completion handler with
 * `asio::error::operation_aborted`. This can be useful in combination with `asio::parallel_group` or
 * `asio::awaitable_operators`, e.g. to perform an action every 100ms while waiting for a server-stream:
 *
 * @snippet client.cpp cancel-safe-server-streaming
 *
 * @tparam CompletionArgs The arguments of the completion signature. E.g. for `asio::steady_timer::async_wait` the
 * completion arguments would be `boost::system::error_code`.
 *
 * @since 1.6.0 (and Boost.Asio 1.77.0)
 */
template <class... CompletionArgs>
class CancelSafe<void(CompletionArgs...)>
{
  private:
    using CompletionSignature = detail::PrependErrorCodeToSignatureT<void(CompletionArgs...)>;

    struct Initiator;

  public:
    /**
     * @brief The type of completion token used to initiate asynchronous operations
     */
    class CompletionToken
    {
      public:
        void operator()(CompletionArgs... completion_args)
        {
            if (auto ch = self.completion_handler.release())
            {
                detail::complete_successfully(std::move(ch), std::move(completion_args)...);
            }
            else
            {
                this->self.result.emplace(std::move(completion_args)...);
            }
        }

      private:
        friend agrpc::CancelSafe<void(CompletionArgs...)>;

        explicit CompletionToken(CancelSafe& self) noexcept : self(self) {}

        CancelSafe& self;
    };

    /**
     * @brief Create a completion token to initiate asynchronous operations
     *
     * Thread-safe
     */
    [[nodiscard]] CompletionToken token() noexcept { return CompletionToken{*this}; }

    /**
     * @brief Is an operation currently running?
     *
     * Thread-safe
     */
    [[nodiscard]] bool is_running() const noexcept { return bool{this->completion_handler}; }

    /**
     * @brief Wait for the asynchronous operation to complete
     *
     * Only one call to `wait()` may be outstanding at a time. Waiting for an already completed operation will
     * immediately invoke the completion handler in a manner equivalent to using `asio::post`.
     *
     * Thread-unsafe with regards to successful completion of the asynchronous operation.
     *
     * **Per-Operation Cancellation**
     *
     * All. Upon cancellation, the asynchronous operation continues to run.
     *
     * @param token Completion token that matches the completion args. Either `void(error_code, CompletionArgs...)` if
     * the first argument in CompletionArgs is not `error_code` or `void(CompletionArgs...)` otherwise.
     */
    template <class CompletionToken>
    auto wait(CompletionToken token)
    {
        assert(!completion_handler && "Can only wait again when the previous wait has been cancelled or completed");
        return asio::async_initiate<CompletionToken, CompletionSignature>(Initiator{*this}, token);
    }

  private:
    struct Initiator
    {
        CancelSafe& self;

        template <class CompletionHandler>
        void operator()(CompletionHandler&& ch)
        {
            if (self.result)
            {
                auto executor = asio::get_associated_executor(ch);
                const auto allocator = asio::get_associated_allocator(ch);
                auto local_result{std::move(*self.result)};
                self.result.reset();
                detail::post_with_allocator(
                    std::move(executor),
                    [local_result = std::move(local_result), ch = std::forward<CompletionHandler>(ch)]() mutable
                    {
                        detail::invoke_successfully_from_tuple(std::move(ch), std::move(local_result));
                    },
                    allocator);
                return;
            }
            auto cancellation_slot = asio::get_associated_cancellation_slot(ch);
            self.emplace_completion_handler(std::forward<CompletionHandler>(ch));
            self.install_cancellation_handler(cancellation_slot);
        }
    };

    struct CancellationHandler
    {
        CancelSafe& self;

        void operator()(asio::cancellation_type type)
        {
            if (static_cast<bool>(type & asio::cancellation_type::all))
            {
                if (auto ch = self.completion_handler.release())
                {
                    detail::complete_operation_aborted(std::move(ch), CompletionArgs{}...);
                }
            }
        }
    };

    template <class CompletionHandler>
    void emplace_completion_handler(CompletionHandler&& ch)
    {
        completion_handler
            .template emplace<detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>>(
                std::forward<CompletionHandler>(ch));
    }

    template <class CancellationSlot>
    void install_cancellation_handler(CancellationSlot& cancellation_slot)
    {
        if (cancellation_slot.is_connected())
        {
            cancellation_slot.assign(CancellationHandler{*this});
        }
    }

    detail::AtomicTypeErasedCompletionHandler<CompletionSignature> completion_handler{};
    std::optional<std::tuple<CompletionArgs...>> result;
};

/**
 * @brief CancelSafe templated on `void(bool)`
 */
using GrpcCancelSafe = agrpc::CancelSafe<void(bool)>;

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_CANCELSAFE_HPP
