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
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/utility.hpp>

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class ManualResetEvent;
class ManualResetEventSender;

template <class Receiver>
struct ManualResetEventRunningOperationState;

struct ManualResetEventOperationStateBase
{
    using SetValue = void (*)(ManualResetEventOperationStateBase*) noexcept;

    void set_value() noexcept { set_value_(this); }

    ManualResetEvent& event_;
    SetValue set_value_;
};

class ManualResetEvent
{
  public:
    void set() noexcept
    {
        auto* const op = op_.exchange(signalled_state(), std::memory_order_acq_rel);
        if (op == signalled_state() || op == nullptr)
        {
            return;
        }
        op->set_value();
    }

    [[nodiscard]] bool ready() const noexcept
    {
        return op_.load(std::memory_order_acquire) == static_cast<const void*>(this);
    }

    void reset() noexcept
    {
        auto* state = signalled_state();
        (void)op_.compare_exchange_strong(state, nullptr, std::memory_order_acq_rel);
    }

    [[nodiscard]] ManualResetEventSender wait() noexcept;

  private:
    template <class Receiver>
    friend struct ManualResetEventRunningOperationState;

    ManualResetEventOperationStateBase* signalled_state()
    {
        return reinterpret_cast<ManualResetEventOperationStateBase*>(this);
    }

    std::atomic<ManualResetEventOperationStateBase*> op_{};
};

template <class Receiver>
[[nodiscard]] bool check_start_conditions(Receiver& receiver)
{
    if (detail::exec::get_stop_token(receiver).stop_requested())
    {
        detail::exec::set_done(static_cast<Receiver&&>(receiver));
        return false;
    }
    return true;
}

template <class Receiver, detail::DeallocateOnComplete>
void set_value_impl(ManualResetEventOperationStateBase* base) noexcept;

template <detail::DeallocateOnComplete>
struct DeallocateOnCompleteArg
{
};

template <class Receiver>
struct ManualResetEventRunningOperationState : ManualResetEventOperationStateBase
{
    struct StopFunction
    {
        template <class... T>
        void operator()(T&&...) const
        {
            ManualResetEventOperationStateBase* op = op_;
            if (event_.op_.compare_exchange_strong(op, nullptr, std::memory_order_acq_rel))
            {
                detail::exec::set_done(static_cast<Receiver&&>(op_->receiver()));
            }
        }

        ManualResetEventRunningOperationState* op_;
        ManualResetEvent& event_;
    };

    template <class R, detail::DeallocateOnComplete Deallocate>
    ManualResetEventRunningOperationState(R&& receiver, ManualResetEvent& event, DeallocateOnCompleteArg<Deallocate>)
        : ManualResetEventOperationStateBase{event, &detail::set_value_impl<Receiver, Deallocate>},
          impl_(static_cast<R&&>(receiver))
    {
    }

    void start() noexcept
    {
        stop_callback().emplace(detail::exec::get_stop_token(receiver()), StopFunction{this, event_});
        event_.op_.store(this, std::memory_order_release);
    }

    auto& receiver() noexcept { return impl_.first(); }

    auto& stop_callback() noexcept { return impl_.second(); }

    detail::CompressedPair<Receiver, detail::StopCallbackLifetime<Receiver, StopFunction>> impl_;
};

template <class Receiver, detail::DeallocateOnComplete Deallocate>
void set_value_impl(ManualResetEventOperationStateBase* base) noexcept
{
    auto* self = static_cast<ManualResetEventRunningOperationState<Receiver>*>(base);
    if constexpr (Deallocate == detail::DeallocateOnComplete::YES)
    {
        Receiver local_receiver{static_cast<Receiver&&>(self->receiver())};
        detail::destroy_deallocate(self, detail::exec::get_allocator(local_receiver));
        detail::satisfy_receiver(static_cast<Receiver&&>(local_receiver));
    }
    else
    {
        detail::satisfy_receiver(static_cast<Receiver&&>(self->receiver()));
    }
}

template <class Receiver>
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
        if (detail::check_start_conditions(state.receiver()))
        {
            state.start();
        }
    }

  private:
    friend ManualResetEventSender;

    template <class R>
    ManualResetEventOperationState(R&& receiver, ManualResetEvent& event)
        : state(static_cast<R&&>(receiver), event, DeallocateOnCompleteArg<DeallocateOnComplete::NO>{})
    {
    }

    ManualResetEventRunningOperationState<Receiver> state;
};

class ManualResetEventSender : public detail::SenderOf<void()>
{
  public:
    template <class R>
    [[nodiscard]] auto connect(R&& receiver) && noexcept(detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<R>)
    {
        return ManualResetEventOperationState<detail::RemoveCrefT<R>>{static_cast<R&&>(receiver), event_};
    }

  private:
    friend ManualResetEvent;

    explicit ManualResetEventSender(ManualResetEvent& event) : event_(event) {}

    ManualResetEvent& event_;
};

inline ManualResetEventSender ManualResetEvent::wait() noexcept { return ManualResetEventSender{*this}; }
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
