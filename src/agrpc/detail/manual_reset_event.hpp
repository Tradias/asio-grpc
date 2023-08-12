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

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/utility.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class ManualResetEvent
{
  private:
    struct OperationStateBase
    {
        using SetValue = void (*)(OperationStateBase*) noexcept;

        void set_value() noexcept { set_value_(this); }

        ManualResetEvent& event_;
        SetValue set_value_;
    };

    template <class Receiver>
    class OperationState : private OperationStateBase
    {
      private:
        struct StopFunction
        {
            template <class... T>
            void operator()(T&&...) const
            {
                OperationStateBase* op = op_;
                if (event_.pending_.compare_exchange_strong(op, nullptr, std::memory_order_acq_rel))
                {
                    detail::exec::set_done(static_cast<Receiver&&>(op_->receiver()));
                }
            }

            OperationState* op_;
            ManualResetEvent& event_;
        };

      public:
        template <class R>
        OperationState(R&& receiver, ManualResetEvent& event)
            : OperationStateBase{event, &set_value_impl}, impl_(static_cast<R&&>(receiver))
        {
        }

        void start() noexcept
        {
            auto stop_token = detail::exec::get_stop_token(receiver());
            if (stop_token.stop_requested())
            {
                detail::exec::set_done(static_cast<Receiver&&>(receiver()));
                return;
            }
            auto* const pending = event_.pending_.load(std::memory_order_acquire);
            if (pending == event_.signalled_state())
            {
                detail::satisfy_receiver(static_cast<Receiver&&>(receiver()));
                return;
            }
            stop_callback().emplace(static_cast<decltype(stop_token)&&>(stop_token), StopFunction{this, event_});
            event_.pending_.store(this, std::memory_order_release);
        }

      private:
        auto& receiver() noexcept { return impl_.first(); }

        auto& stop_callback() noexcept { return impl_.second(); }

        static void set_value_impl(OperationStateBase* base) noexcept
        {
            auto& self = *static_cast<OperationState*>(base);
            detail::satisfy_receiver(static_cast<Receiver&&>(self.receiver()));
        }

        detail::CompressedPair<Receiver, detail::StopCallbackLifetime<Receiver, StopFunction>> impl_;
    };

    class Sender : public detail::SenderOf<void()>
    {
      public:
        template <class R>
        auto connect(R&& receiver)
        {
            return OperationState<detail::RemoveCrefT<R>>{static_cast<R&&>(receiver), event_};
        }

      private:
        friend ManualResetEvent;

        explicit Sender(ManualResetEvent& event) : event_(event) {}

        ManualResetEvent& event_;
    };

  public:
    void set() noexcept
    {
        auto* const pending = pending_.exchange(signalled_state(), std::memory_order_acq_rel);
        if (pending == signalled_state() || pending == nullptr)
        {
            return;
        }
        pending->set_value();
    }

    bool ready() const noexcept { return pending_.load(std::memory_order_acquire) == static_cast<const void*>(this); }

    void reset() noexcept
    {
        auto* state = signalled_state();
        (void)pending_.compare_exchange_strong(state, nullptr, std::memory_order_acq_rel);
    }

    [[nodiscard]] auto wait() noexcept { return Sender{*this}; }

  private:
    OperationStateBase* signalled_state() { return reinterpret_cast<OperationStateBase*>(this); }

    std::atomic<OperationStateBase*> pending_{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
