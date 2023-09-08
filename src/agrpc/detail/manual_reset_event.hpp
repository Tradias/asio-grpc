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

template <class Signature>
struct ManualResetEventOperationStateBase;

template <class... Args>
struct ManualResetEventOperationStateBase<void(Args...)>
{
    using SetValue = void (*)(ManualResetEventOperationStateBase*, Args...);

    void set_value(Args... args) noexcept { set_value_(this, args...); }

    ManualResetEvent<void(Args...)>& event_;
    SetValue set_value_;
};

template <class... Args>
class ManualResetEvent<void(Args...)> : private detail::Tuple<Args...>
{
  private:
    using Signature = void(Args...);
    using Op = ManualResetEventOperationStateBase<Signature>;

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

    void store_value(Args... args) { static_cast<detail::Tuple<Args...>&>(*this) = detail::Tuple<Args...>{args...}; }

    auto* signalled_state() const { return const_cast<Op*>(reinterpret_cast<const Op*>(this)); }

    std::atomic<Op*> op_{};
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

template <class Signature, class Receiver, detail::DeallocateOnComplete>
inline constexpr bool set_value_impl{};

template <class Signature, class Receiver>
struct ManualResetEventRunningOperationState : ManualResetEventOperationStateBase<Signature>
{
    struct StopFunction
    {
        template <class... T>
        void operator()(T&&...) const
        {
            ManualResetEventOperationStateBase<Signature>* op = op_;
            if (event_.op_.compare_exchange_strong(op, nullptr, std::memory_order_acq_rel))
            {
                detail::exec::set_done(static_cast<Receiver&&>(op_->receiver()));
            }
        }

        ManualResetEventRunningOperationState* op_;
        ManualResetEvent<Signature>& event_;
    };

    template <class R, detail::DeallocateOnComplete Deallocate>
    ManualResetEventRunningOperationState(R&& receiver, ManualResetEvent<Signature>& event,
                                          DeallocateOnCompleteArg<Deallocate>)
        : ManualResetEventOperationStateBase<Signature>{event, detail::set_value_impl<Signature, Receiver, Deallocate>},
          impl_(static_cast<R&&>(receiver))
    {
    }

    void start() noexcept
    {
        stop_callback().emplace(detail::exec::get_stop_token(receiver()), StopFunction{this, this->event_});
        this->event_.op_.store(this, std::memory_order_release);
    }

    auto& receiver() noexcept { return impl_.first(); }

    auto& stop_callback() noexcept { return impl_.second(); }

    detail::CompressedPair<Receiver, detail::StopCallbackLifetime<exec::stop_token_type_t<Receiver&>, StopFunction>>
        impl_;
};

template <class... Args, class Receiver, detail::DeallocateOnComplete Deallocate>
inline constexpr auto set_value_impl<void(Args...), Receiver, Deallocate> =
    +[](ManualResetEventOperationStateBase<void(Args...)>* base, Args... args)
{
    auto* self = static_cast<ManualResetEventRunningOperationState<void(Args...), Receiver>*>(base);
    if constexpr (Deallocate == detail::DeallocateOnComplete::YES)
    {
        Receiver local_receiver{static_cast<Receiver&&>(self->receiver())};
        detail::destroy_deallocate(self, detail::exec::get_allocator(local_receiver));
        detail::satisfy_receiver(static_cast<Receiver&&>(local_receiver), args...);
    }
    else
    {
        detail::satisfy_receiver(static_cast<Receiver&&>(self->receiver()), args...);
    }
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
        if (detail::check_start_conditions(state.receiver()))
        {
            state.start();
        }
    }

  private:
    friend ManualResetEventSender<Signature>;

    template <class R>
    ManualResetEventOperationState(R&& receiver, ManualResetEvent<Signature>& event)
        : state(static_cast<R&&>(receiver), event, DeallocateOnCompleteArg<DeallocateOnComplete::NO>{})
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
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
