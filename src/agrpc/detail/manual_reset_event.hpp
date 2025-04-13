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

#ifndef AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
#define AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_utils.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/prepend_error_code.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/tuple.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/use_sender.hpp>

#include <atomic>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature, template <class...> class Storage>
class ManualResetEventSender;

template <class Signature, template <class...> class Storage, class Receiver>
struct ManualResetEventRunningOperationState;

template <template <class...> class Storage, class... Args>
struct ManualResetEventOperationBase<void(Args...), Storage>
{
    using Complete = void (*)(ManualResetEventOperationBase*);

    void complete() noexcept { complete_(this); }

    BasicManualResetEvent<void(Args...), Storage>& event_;
    Complete complete_;
};

template <class... Args>
class ManualResetEventTupleStorage : private detail::Tuple<Args...>
{
  public:
    void set_value(Args&&... args)
    {
        static_cast<detail::Tuple<Args...>&>(*this) = detail::Tuple<Args...>{static_cast<Args&&>(args)...};
    }

    auto&& get_value() && noexcept { return static_cast<detail::Tuple<Args...>&&>(*this); }
};

template <template <class...> class StorageT, class... Args>
class BasicManualResetEvent<void(Args...), StorageT> : private StorageT<Args...>
{
  public:
    using Storage = StorageT<Args...>;
    using Signature = void(Args...);

  private:
    using Op = ManualResetEventOperationBase<Signature, StorageT>;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    struct InitiateWait
    {
        template <class CompletionHandler, class IOExecutor>
        void operator()(CompletionHandler&& completion_handler, const IOExecutor& io_executor) const
        {
            if (auto& event = event_; event.ready())
            {
                detail::complete_immediately(
                    static_cast<CompletionHandler&&>(completion_handler),
                    [&event](auto&& ch)
                    {
                        detail::prepend_error_code_and_apply(static_cast<decltype(ch)&&>(ch),
                                                             static_cast<Storage&&>(event).get_value());
                    },
                    io_executor);
                return;
            }
            const auto allocator = asio::get_associated_allocator(completion_handler);
            detail::allocate<ManualResetEventOperation<Signature, StorageT, detail::RemoveCrefT<CompletionHandler>>>(
                allocator, static_cast<CompletionHandler&&>(completion_handler), event_)
                .release();
        }

        BasicManualResetEvent& event_;
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
        Storage::set_value(static_cast<Args&&>(args)...);
        op->complete();
    }

    [[nodiscard]] bool ready() const noexcept { return op_.load(std::memory_order_acquire) == signalled_state(); }

    void reset() noexcept
    {
        auto* expected = signalled_state();
        op_.compare_exchange_strong(expected, nullptr, std::memory_order_release);
    }

    template <class IOExecutor>
    [[nodiscard]] ManualResetEventSender<Signature, StorageT> wait(agrpc::UseSender, const IOExecutor&) noexcept
    {
        return wait();
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class CompletionToken, class IOExecutor>
    auto wait(CompletionToken&& token, const IOExecutor& io_executor)
    {
        using Sig = detail::PrependErrorCodeToSignatureT<Signature>;
        return asio::async_initiate<CompletionToken, Sig>(InitiateWait{*this}, token, io_executor);
    }
#endif

    using Storage::get_value;

  private:
    template <class, template <class...> class, class>
    friend struct ManualResetEventRunningOperationState;

    template <class, template <class...> class, class>
    friend struct ManualResetEventOperation;

    [[nodiscard]] bool compare_exchange(Op* op) noexcept
    {
        return op_.compare_exchange_strong(op, nullptr, std::memory_order_acq_rel);
    }

    auto* signalled_state() const { return const_cast<Op*>(reinterpret_cast<const Op*>(this)); }

    [[nodiscard]] ManualResetEventSender<Signature, StorageT> wait() noexcept;

    std::atomic<Op*> op_{};
};

template <class... Args, template <class...> class Storage, class Receiver>
struct ManualResetEventRunningOperationState<void(Args...), Storage, Receiver>
    : ManualResetEventOperationBase<void(Args...), Storage>
{
    using Signature = void(Args...);
    using Base = ManualResetEventOperationBase<Signature, Storage>;
    using Event = BasicManualResetEvent<Signature, Storage>;

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

    using StopToken = exec::stop_token_type_t<Receiver&>;
    using StopCallback = detail::StopCallbackLifetime<StopToken, StopFunction>;

    static void complete_impl(Base* base)
    {
        auto& self = *static_cast<ManualResetEventRunningOperationState*>(base);
        self.stop_callback().reset();
        self.complete();
    }

    template <class R>
    ManualResetEventRunningOperationState(R&& receiver, Event& event)
        : Base{event, &complete_impl}, impl_(static_cast<R&&>(receiver))
    {
    }

    void start(StopToken&& stop_token) noexcept
    {
        stop_callback().emplace(static_cast<StopToken&&>(stop_token), StopFunction{*this});
        this->event_.op_.store(this, std::memory_order_release);
    }

    void complete() noexcept
    {
        detail::apply(
            [&](Args&&... args)
            {
                exec::set_value(static_cast<Receiver&&>(receiver()), static_cast<Args&&>(args)...);
            },
            static_cast<Event&&>(this->event_).get_value());
    }

    auto& receiver() noexcept { return impl_.first(); }

    auto& stop_callback() noexcept { return impl_.second(); }

    detail::CompressedPair<Receiver, StopCallback> impl_;
};

template <class Signature, template <class...> class Storage, class Receiver>
class ManualResetEventOperationState
{
  public:
    void start() noexcept
    {
        if (state_.event_.ready())
        {
            state_.complete();
            return;
        }
        auto stop_token = exec::get_stop_token(state_.receiver());
        if (stop_token.stop_requested())
        {
            exec::set_done(static_cast<Receiver&&>(state_.receiver()));
            return;
        }
        state_.start(std::move(stop_token));
    }

#ifdef AGRPC_STDEXEC
    friend void tag_invoke(stdexec::start_t, ManualResetEventOperationState& o) noexcept { o.start(); }
#endif

  private:
    friend ManualResetEventSender<Signature, Storage>;

    template <class R>
    ManualResetEventOperationState(R&& receiver, BasicManualResetEvent<Signature, Storage>& event)
        : state_(static_cast<R&&>(receiver), event)
    {
    }

    ManualResetEventRunningOperationState<Signature, Storage, Receiver> state_;
};

template <class Signature, template <class...> class Storage>
class [[nodiscard]] ManualResetEventSender : public detail::SenderOf<Signature>
{
  public:
    template <class R>
    [[nodiscard]] auto connect(R&& receiver) && noexcept(detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<R>)
        -> ManualResetEventOperationState<Signature, Storage, detail::RemoveCrefT<R>>
    {
        return {static_cast<R&&>(receiver), event_};
    }

#ifdef AGRPC_STDEXEC
    template <class Receiver>
    friend auto tag_invoke(stdexec::connect_t, ManualResetEventSender&& s, Receiver&& r) noexcept(
        noexcept(static_cast<ManualResetEventSender&&>(s).connect(static_cast<Receiver&&>(r))))
    {
        return static_cast<ManualResetEventSender&&>(s).connect(static_cast<Receiver&&>(r));
    }
#endif

  private:
    friend ManualResetEvent<Signature>;

    explicit ManualResetEventSender(ManualResetEvent<Signature>& event) noexcept : event_(event) {}

    ManualResetEvent<Signature>& event_;
};

template <template <class...> class StorageT, class... Args>
inline ManualResetEventSender<void(Args...), StorageT> BasicManualResetEvent<void(Args...), StorageT>::wait() noexcept
{
    return ManualResetEventSender<void(Args...), StorageT>{*this};
}
}

AGRPC_NAMESPACE_END

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/manual_reset_event_operation.hpp>
#endif

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_HPP
