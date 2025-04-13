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

#ifndef AGRPC_DETAIL_MANUAL_RESET_EVENT_OPERATION_HPP
#define AGRPC_DETAIL_MANUAL_RESET_EVENT_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/prepend_error_code.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>

#include <agrpc/detail/asio_macros.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class... Args, class CompletionHandler>
struct ManualResetEventOperation<void(Args...), CompletionHandler>
    : public ManualResetEventOperationBase<void(Args...)>,
      private detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandler>>
{
    using Signature = void(Args...);
    using Base = ManualResetEventOperationBase<Signature>;
    using WorkTracker = detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandler>>;
    using Event = ManualResetEventBase<Signature>;

    struct StopFunction
    {
        explicit StopFunction(ManualResetEventOperation& op) noexcept : op_(op) {}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        void operator()(asio::cancellation_type type) const
        {
            if (static_cast<bool>(type & asio::cancellation_type::all))
            {
                auto* op = static_cast<Base*>(&op_);
                if (op->event_.compare_exchange(op))
                {
                    op_.cancel();
                }
            }
        }
#endif

        ManualResetEventOperation& op_;
    };

    static void complete_impl(Base* base, Args... args)
    {
        auto& self = *static_cast<ManualResetEventOperation*>(base);
        self.complete(static_cast<Args&&>(args)...);
    }

    template <class Ch>
    ManualResetEventOperation(Ch&& ch, Event& event)
        : Base{event, &complete_impl},
          WorkTracker(asio::get_associated_executor(ch)),
          completion_handler_(static_cast<Ch&&>(ch))
    {
        emplace_stop_callback();
        this->event_.op_.store(this, std::memory_order_release);
    }

    void emplace_stop_callback()
    {
        if constexpr (detail::IS_STOP_EVER_POSSIBLE_V<detail::CancellationSlotT<CompletionHandler&>>)
        {
            if (auto slot = detail::get_cancellation_slot(completion_handler_); slot.is_connected())
            {
                slot.template emplace<StopFunction>(*this);
            }
        }
    }

    template <class... TArgs>
    void complete(detail::ErrorCode&& ec, TArgs&&... args)
    {
        detail::AllocationGuard ptr{*this, asio::get_associated_allocator(completion_handler_)};
        detail::dispatch_complete(ptr, static_cast<detail::ErrorCode&&>(ec), static_cast<TArgs&&>(args)...);
    }

    template <class... TArgs>
    void complete(TArgs&&... args)
    {
        complete(detail::ErrorCode{}, static_cast<TArgs&&>(args)...);
    }

    void cancel()
    {
        detail::PrependErrorCodeToSignature<Signature>::invoke_with_default_args(
            [&](detail::ErrorCode&& ec, auto&&... args)
            {
                complete(static_cast<detail::ErrorCode&&>(ec), static_cast<decltype(args)&&>(args)...);
            },
            detail::operation_aborted_error_code());
    }

    CompletionHandler& completion_handler() noexcept { return completion_handler_; }

    WorkTracker& work_tracker() noexcept { return *this; }

    CompletionHandler completion_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_OPERATION_HPP
