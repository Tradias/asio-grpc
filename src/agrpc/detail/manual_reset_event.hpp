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
#include <agrpc/detail/cancel_safe.hpp>
#include <agrpc/detail/config.hpp>
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

template <class Signature, class Receiver>
struct ManualResetEventOperation;

template <class Signature>
struct ManualResetEventOperationBase;

template <class... Args>
struct ManualResetEventOperationBase<void(Args...)>
{
    using Complete = void (*)(ManualResetEventOperationBase*);

    void complete() noexcept { complete_(this); }

    ManualResetEvent<void(Args...)>& event_;
    Complete complete_;
};

template <class CompletionHandler, class... Args, std::size_t... I>
void prepend_error_code_and_apply_impl(CompletionHandler&& ch, detail::Tuple<Args...>&& args,
                                       const std::index_sequence<I...>&)
{
    static_cast<CompletionHandler&&>(ch)(detail::ErrorCode{},
                                         detail::get<I>(static_cast<detail::Tuple<Args...>&&>(args))...);
}

template <class CompletionHandler, class... Args, std::size_t... I>
void prepend_error_code_and_apply_impl(CompletionHandler&& ch, detail::Tuple<detail::ErrorCode, Args...>&& args,
                                       const std::index_sequence<I...>&)
{
    static_cast<CompletionHandler&&>(ch)(
        detail::get<I>(static_cast<detail::Tuple<detail::ErrorCode, Args...>&&>(args))...);
}

template <class CompletionHandler, class... Args>
void prepend_error_code_and_apply(CompletionHandler&& ch, detail::Tuple<Args...>&& args)
{
    detail::prepend_error_code_and_apply_impl(static_cast<CompletionHandler&&>(ch),
                                              static_cast<detail::Tuple<Args...>&&>(args),
                                              std::make_index_sequence<sizeof...(Args)>{});
}

template <class... Args>
class ManualResetEvent<void(Args...)> : private detail::Tuple<Args...>
{
  private:
    using Signature = void(Args...);
    using Op = ManualResetEventOperationBase<Signature>;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    struct InitiateWait
    {
        template <class CompletionHandler, class IOExecutor>
        void operator()(CompletionHandler&& completion_handler, const IOExecutor& io_executor) const
        {
            const auto allocator = asio::get_associated_allocator(completion_handler);
            auto& event = event_;
            if (event.ready())
            {
                auto executor = asio::get_associated_executor(completion_handler, io_executor);
                detail::post_with_allocator(
                    std::move(executor),
                    [&event, ch = static_cast<CompletionHandler&&>(completion_handler)]() mutable
                    {
                        detail::prepend_error_code_and_apply(static_cast<CompletionHandler&&>(ch),
                                                             static_cast<ManualResetEvent&&>(event).args());
                    },
                    allocator);
                return;
            }
            using Ch = detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>;
            using Operation = ManualResetEventOperation<Signature, Ch>;
            detail::allocate<Operation>(allocator, static_cast<CompletionHandler&&>(completion_handler), event_)
                .release();
        }

        ManualResetEvent& event_;
    };
#endif

  public:
    void set(Args&&... args)
    {
        auto* const op = op_.exchange(signalled_state(), std::memory_order_acq_rel);
        if (op == signalled_state() || op == nullptr)
        {
            return;
        }
        store_value(static_cast<Args&&>(args)...);
        op->complete();
    }

    [[nodiscard]] bool ready() const noexcept { return op_.load(std::memory_order_acquire) == signalled_state(); }

    void reset() noexcept
    {
        auto* expected = signalled_state();
        op_.compare_exchange_strong(expected, nullptr, std::memory_order_release);
    }

    [[nodiscard]] ManualResetEventSender<Signature> wait() noexcept;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class CompletionToken, class IOExecutor>
    auto wait(CompletionToken token, const IOExecutor& io_executor)
    {
        using Sig = detail::PrependErrorCodeToSignatureT<Signature>;
        return asio::async_initiate<CompletionToken, Sig>(InitiateWait{*this}, token, io_executor);
    }
#endif

    auto&& args() && noexcept { return static_cast<detail::Tuple<Args...>&&>(*this); }

  private:
    template <class, class>
    friend struct ManualResetEventRunningOperationState;

    template <class, class>
    friend struct ManualResetEventOperation;

    void store_value(Args&&... args)
    {
        static_cast<detail::Tuple<Args...>&>(*this) = detail::Tuple<Args...>{static_cast<Args&&>(args)...};
    }

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
    using Event = ManualResetEvent<Signature>;

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

    static void complete_impl(Base* base)
    {
        auto& self = *static_cast<ManualResetEventRunningOperationState*>(base);
        self.stop_callback().reset();
        self.complete();
    }

    template <class R>
    ManualResetEventRunningOperationState(R&& receiver, ManualResetEvent<Signature>& event)
        : Base{event, &complete_impl}, impl_(static_cast<R&&>(receiver))
    {
    }

    void start() noexcept
    {
        stop_callback().emplace(exec::get_stop_token(receiver()), StopFunction{*this});
        this->event_.op_.store(this, std::memory_order_release);
    }

    void complete()
    {
        detail::apply(
            [&](Args&&... args)
            {
                detail::satisfy_receiver(static_cast<Receiver&&>(receiver()), static_cast<Args&&>(args)...);
            },
            static_cast<Event&&>(this->event_).args());
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
            state.complete();
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
        -> ManualResetEventOperationState<Signature, detail::RemoveCrefT<R>>
    {
        return {static_cast<R&&>(receiver), event_};
    }

  private:
    friend ManualResetEvent<Signature>;

    explicit ManualResetEventSender(ManualResetEvent<Signature>& event) noexcept : event_(event) {}

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
    using Event = ManualResetEvent<Signature>;

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

    static void complete_impl(Base* base)
    {
        auto& self = *static_cast<ManualResetEventOperation*>(base);
        detail::prepend_error_code_and_apply(
            [&](auto&&... args)
            {
                self.complete(static_cast<decltype(args)&&>(args)...);
            },
            static_cast<Event&&>(self.event_).args());
    }

    template <class Ch>
    ManualResetEventOperation(Ch&& ch, ManualResetEvent<Signature>& event)
        : Base{event, &complete_impl}, completion_handler_(static_cast<Ch&&>(ch))
    {
        emplace_stop_callback();
        this->event_.op_.store(this, std::memory_order_release);
    }

    void emplace_stop_callback()
    {
        if constexpr (detail::IS_STOP_EVER_POSSIBLE_V<exec::stop_token_type_t<CompletionHandler&>>)
        {
            if (auto stop_token = exec::get_stop_token(completion_handler_); detail::stop_possible(stop_token))
            {
                stop_token.template emplace<StopFunction>(*this);
            }
        }
    }

    template <class... TArgs>
    void complete(TArgs&&... args)
    {
        detail::AllocationGuard ptr{this, exec::get_allocator(completion_handler_)};
        auto handler{static_cast<CompletionHandler&&>(completion_handler_)};
        ptr.reset();
        static_cast<CompletionHandler&&>(handler)(static_cast<TArgs&&>(args)...);
    }

    void cancel()
    {
        detail::PrependErrorCodeToSignature<Signature>::invoke_with_default_args(
            [&](auto&& ec, auto&&... args)
            {
                complete(ec, args...);
            },
            detail::operation_aborted_error_code());
    }

    CompletionHandler completion_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
