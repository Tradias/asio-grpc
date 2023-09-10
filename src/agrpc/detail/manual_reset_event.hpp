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

#ifndef AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
#define AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/tuple.hpp>
#include <agrpc/detail/utility.hpp>

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
class ManualResetEvent;

template <class Signature>
class ManualResetEventSender;

template <class Signature, class Receiver>
struct ManualResetEventRunningOperationState;

template <class Signature, class Receiver>
struct ManualResetEventOperation;

template <class Signature>
struct ManualResetEventOperationBase;

template <class... Args>
struct ManualResetEventOperationBase<void(Args...)>
{
    using SetValue = void (*)(ManualResetEventOperationBase*, Args...);

    void set_value(Args... args) noexcept { set_value_(this, args...); }

    ManualResetEvent<void(Args...)>& event_;
    SetValue set_value_;
};

template <class... Args>
class ManualResetEvent<void(Args...)> : private detail::Tuple<Args...>
{
  private:
    using Signature = void(Args...);
    using Op = ManualResetEventOperationBase<Signature>;

  public:
    void set(Args... args)
    {
        auto* const op = op_.exchange(signalled_state(), std::memory_order_acq_rel);
        if (op == signalled_state() || op == nullptr)
        {
            return;
        }
        store_value(args...);
        op->set_value(args...);
    }

    [[nodiscard]] bool ready() const noexcept { return op_.load(std::memory_order_acquire) == signalled_state(); }

    void reset() noexcept { op_.store(nullptr, std::memory_order_release); }

    [[nodiscard]] ManualResetEventSender<Signature> wait() noexcept;

    const detail::Tuple<Args...>& args() const noexcept { return *this; }

  private:
    template <class, class>
    friend struct ManualResetEventRunningOperationState;

    template <class, class>
    friend struct ManualResetEventOperation;

    void store_value(Args... args) { static_cast<detail::Tuple<Args...>&>(*this) = detail::Tuple<Args...>{args...}; }

    [[nodiscard]] bool compare_exchange(Op* op) noexcept
    {
        return op_.compare_exchange_strong(op, nullptr, std::memory_order_acq_rel);
    }

    auto* signalled_state() const { return const_cast<Op*>(reinterpret_cast<const Op*>(this)); }

    std::atomic<Op*> op_{};
};

template <class... Args, class Receiver>
struct ManualResetEventRunningOperationState<void(Args...), Receiver> : ManualResetEventOperationBase<void(Args...)>
{
    using Signature = void(Args...);
    using Base = ManualResetEventOperationBase<Signature>;

    struct StopFunction
    {
        void operator()() const
        {
            auto* op = static_cast<Base*>(&op_);
            op_.stop_callback().reset();
            if (op->event_.compare_exchange(op))
            {
                exec::set_done(static_cast<Receiver&&>(op_.receiver()));
            }
        }

        ManualResetEventRunningOperationState& op_;
    };

    using StopCallback = detail::StopCallbackLifetime<exec::stop_token_type_t<Receiver&>, StopFunction>;

    static void set_value_impl(Base* base, Args... args)
    {
        auto& self = *static_cast<ManualResetEventRunningOperationState*>(base);
        self.stop_callback().reset();
        detail::satisfy_receiver(static_cast<Receiver&&>(self.receiver()), args...);
    }

    template <class R>
    ManualResetEventRunningOperationState(R&& receiver, ManualResetEvent<Signature>& event)
        : Base{event, &set_value_impl}, impl_(static_cast<R&&>(receiver))
    {
    }

    void start() noexcept
    {
        stop_callback().emplace(exec::get_stop_token(receiver()), StopFunction{*this});
        this->event_.op_.store(this, std::memory_order_release);
    }

    auto& receiver() noexcept { return impl_.first(); }

    auto& stop_callback() noexcept { return impl_.second(); }

    detail::CompressedPair<Receiver, StopCallback> impl_;
};

template <class Signature, class Receiver>
class ManualResetEventOperationState
{
  public:
    void start() noexcept
    {
        if (state.event_.ready())
        {
            detail::satisfy_receiver(static_cast<Receiver&&>(state.receiver()));
            return;
        }
        if (detail::stop_requested(exec::get_stop_token(state.receiver())))
        {
            exec::set_done(static_cast<Receiver&&>(state.receiver()));
            return;
        }
        state.start();
    }

  private:
    friend ManualResetEventSender<Signature>;

    template <class R>
    ManualResetEventOperationState(R&& receiver, ManualResetEvent<Signature>& event)
        : state(static_cast<R&&>(receiver), event)
    {
    }

    ManualResetEventRunningOperationState<Signature, Receiver> state;
};

template <class Signature>
class ManualResetEventSender : public detail::SenderOf<void()>
{
  public:
    template <class R>
    [[nodiscard]] auto connect(R&& receiver) && noexcept(detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<R>)
    {
        return ManualResetEventOperationState<Signature, detail::RemoveCrefT<R>>{static_cast<R&&>(receiver), event_};
    }

  private:
    friend ManualResetEvent<Signature>;

    explicit ManualResetEventSender(ManualResetEvent<Signature>& event) : event_(event) {}

    ManualResetEvent<Signature>& event_;
};

template <class... Args>
inline ManualResetEventSender<void(Args...)> ManualResetEvent<void(Args...)>::wait() noexcept
{
    return ManualResetEventSender<void(Args...)>{*this};
}

template <class... Args, class CompletionHandler>
struct ManualResetEventOperation<void(Args...), CompletionHandler> : ManualResetEventOperationBase<void(Args...)>
{
    using Signature = void(Args...);
    using Base = ManualResetEventOperationBase<Signature>;

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

    static void set_value_impl(Base* base, Args... args)
    {
        auto* self = static_cast<ManualResetEventOperation*>(base);
        detail::AllocationGuard ptr{self, exec::get_allocator(self->completion_handler_)};
        auto handler{static_cast<CompletionHandler&&>(self->completion_handler_)};
        ptr.reset();
        static_cast<CompletionHandler&&>(handler)(detail::ErrorCode{}, static_cast<Args&&>(args)...);
    }

    template <class Ch>
    ManualResetEventOperation(Ch&& ch, ManualResetEvent<Signature>& event)
        : Base{event, &set_value_impl}, completion_handler_(static_cast<Ch&&>(ch))
    {
        detail::emplace_stop_callback<StopFunction>(*this,
                                                    [&]() -> ManualResetEventOperation&
                                                    {
                                                        return *this;
                                                    });
        this->event_.op_.store(this, std::memory_order_release);
    }

    void cancel()
    {
        detail::AllocationGuard ptr{this, exec::get_allocator(completion_handler_)};
        auto handler{static_cast<CompletionHandler&&>(completion_handler_)};
        ptr.reset();
        static_cast<CompletionHandler&&>(handler)(detail::operation_aborted_error_code(), Args{}...);
    }

    CompletionHandler& completion_handler() noexcept { return completion_handler_; }

    CompletionHandler completion_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
