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

template <class Signature, class Receiver>
struct ManualResetEventRunningOperationState;

template <class Signature>
class ManualResetEventBase;

template <class... Args>
class ManualResetEventBase<void(Args...)>
{
  private:
    using Signature = void(Args...);
    using Op = ManualResetEventOperationBase<Signature>;

  public:
    [[nodiscard]] bool ready() const noexcept { return op_.load(std::memory_order_acquire) == signalled_state(); }

    void reset() noexcept { op_.store(nullptr, std::memory_order_release); }

  protected:
    [[nodiscard]] Op* signal() noexcept
    {
        auto* const op = this->op_.exchange(signalled_state(), std::memory_order_acq_rel);
        if (op == signalled_state() || op == nullptr)
        {
            return nullptr;
        }
        return op;
    }

  private:
    template <class, class>
    friend struct detail::ManualResetEventRunningOperationState;

    template <class, class>
    friend struct detail::ManualResetEventOperation;

    template <class, template <class...> class>
    friend class detail::BasicManualResetEvent;

    [[nodiscard]] bool compare_exchange(Op* op) noexcept
    {
        return op_.compare_exchange_strong(op, nullptr, std::memory_order_acq_rel);
    }

    [[nodiscard]] bool store(Op* op) noexcept
    {
        Op* n = nullptr;
        return op_.compare_exchange_strong(n, op, std::memory_order_acq_rel);
    }

    auto* signalled_state() const { return const_cast<Op*>(reinterpret_cast<const Op*>(this)); }

    std::atomic<Op*> op_{};
};

template <class... Args>
struct ManualResetEventOperationBase<void(Args...)>
{
    using Complete = void (*)(ManualResetEventOperationBase*, Args...);

    void complete(Args... args) noexcept { complete_(this, static_cast<Args&&>(args)...); }

    ManualResetEventBase<void(Args...)>& event_;
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
class BasicManualResetEvent<void(Args...), StorageT> : private StorageT<Args...>,
                                                       public ManualResetEventBase<void(Args...)>

{
  private:
    using Signature = void(Args...);
    using SignatureWithErrorCode = detail::PrependErrorCodeToSignatureT<void(Args...)>;

  public:
    using Storage = StorageT<Args...>;

  private:
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    struct InitiateWait
    {
        template <class CompletionHandler, class IOExecutor>
        void operator()(CompletionHandler&& completion_handler, const IOExecutor& io_executor) const
        {
            const auto complete_immediately = [&io_executor, &event = event_](auto&& ch)
            {
                detail::complete_immediately(
                    static_cast<CompletionHandler&&>(ch),
                    [&event](auto&& ch)
                    {
                        detail::prepend_error_code_and_apply(static_cast<decltype(ch)&&>(ch),
                                                             static_cast<Storage&&>(event).get_value());
                    },
                    io_executor);
            };
            if (event_.ready())
            {
                complete_immediately(completion_handler);
                return;
            }
            const auto allocator = asio::get_associated_allocator(completion_handler);
            auto ptr = detail::allocate<ManualResetEventOperation<Signature, detail::RemoveCrefT<CompletionHandler>>>(
                allocator, static_cast<CompletionHandler&&>(completion_handler), event_);
            if (event_.store(ptr.get()))
            {
                ptr.release();
                return;
            }
            auto ch = static_cast<CompletionHandler&&>(ptr->completion_handler());
            ptr.reset();
            complete_immediately(ch);
        }

        BasicManualResetEvent& event_;
    };
#endif

  public:
    void set(Args&&... args)
    {
        Storage::set_value(static_cast<Args&&>(args)...);
        auto* const op = this->signal();
        if (op == nullptr)
        {
            return;
        }
        detail::apply(
            [&op](Args&&... args)
            {
                op->complete(static_cast<Args&&>(args)...);
            },
            static_cast<Storage&&>(*this).get_value());
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
        return asio::async_initiate<CompletionToken, SignatureWithErrorCode>(InitiateWait{*this}, token, io_executor);
    }
#endif

  private:
    template <class, template <class...> class, class>
    friend class detail::ManualResetEventOperationState;

    ManualResetEventSender<Signature, StorageT> wait() noexcept;
};

template <class... Args, class Receiver>
struct ManualResetEventRunningOperationState<void(Args...), Receiver> : ManualResetEventOperationBase<void(Args...)>
{
    using Signature = void(Args...);
    using Base = ManualResetEventOperationBase<Signature>;
    using Event = ManualResetEventBase<Signature>;

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

    static void complete_impl(Base* base, Args... args)
    {
        auto& self = *static_cast<ManualResetEventRunningOperationState*>(base);
        self.stop_callback().reset();
        self.complete(static_cast<Args&&>(args)...);
    }

    template <class R>
    ManualResetEventRunningOperationState(R&& receiver, Event& event)
        : Base{event, &complete_impl}, impl_(static_cast<R&&>(receiver))
    {
    }

    bool start(StopToken&& stop_token) noexcept
    {
        stop_callback().emplace(static_cast<StopToken&&>(stop_token), StopFunction{*this});
        return static_cast<bool>(this->event_.store(this));
    }

    void complete(Args&&... args)
    {
        exec::set_value(static_cast<Receiver&&>(receiver()), static_cast<Args&&>(args)...);
    }

    auto& receiver() noexcept { return impl_.first(); }

    auto& stop_callback() noexcept { return impl_.second(); }

    detail::CompressedPair<Receiver, StopCallback> impl_;
};

template <class Signature, template <class...> class Storage, class Receiver>
class ManualResetEventOperationState
{
  private:
    using Event = BasicManualResetEvent<Signature, Storage>;

  public:
    void start() noexcept
    {
        auto stop_token = exec::get_stop_token(state_.receiver());
        if (stop_token.stop_requested())
        {
            exec::set_done(static_cast<Receiver&&>(state_.receiver()));
            return;
        }
        if (!state_.start(std::move(stop_token)))
        {
            complete();
        }
    }

#ifdef AGRPC_STDEXEC
    friend void tag_invoke(stdexec::start_t, ManualResetEventOperationState& o) noexcept { o.start(); }
#endif

  private:
    friend detail::ManualResetEventSender<Signature, Storage>;

    template <class R>
    ManualResetEventOperationState(R&& receiver, Event& event) : state_(static_cast<R&&>(receiver), event)
    {
    }

    void complete()
    {
        detail::apply(
            [&](auto&&... args)
            {
                state_.complete(static_cast<decltype(args)&&>(args)...);
            },
            static_cast<Event&&>(state_.event_).get_value());
    }

    ManualResetEventRunningOperationState<Signature, Receiver> state_;
};

template <class... Args, template <class...> class Storage>
class ManualResetEventSender<void(Args...), Storage> : public detail::SenderOf<void(Args...)>
{
  private:
    using Signature = void(Args...);
    using Event = BasicManualResetEvent<Signature, Storage>;

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
    friend Event;

    explicit ManualResetEventSender(Event& event) noexcept : event_(event) {}

    Event& event_;
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
