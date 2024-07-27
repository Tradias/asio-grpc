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

#ifndef AGRPC_DETAIL_REGISTER_SENDER_RPC_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_SENDER_RPC_HANDLER_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/atomic_bool_stop_context.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/register_rpc_handler_base.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <optional>
#include <variant>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct InlineSchedulerEnv
{
    friend constexpr exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>,
                                                       const InlineSchedulerEnv&) noexcept
    {
        return {};
    }
};

template <class ServerRPC, class RPCHandler, class StopToken>
struct RegisterRPCHandlerSenderOperationBase;

template <class ServerRPC, class RPCHandler>
class [[nodiscard]] RPCHandlerSender : public detail::SenderOf<void()>
{
  private:
    using Service = detail::GetServerRPCServiceT<ServerRPC>;

  public:
    RPCHandlerSender(agrpc::GrpcContext& grpc_context, Service& service, RPCHandler&& rpc_handler)
        : grpc_context_(grpc_context), service_(service), rpc_handler_(static_cast<RPCHandler&&>(rpc_handler))
    {
    }

    template <class Receiver>
    auto connect(Receiver&& receiver) && noexcept(detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                                  std::is_nothrow_move_constructible_v<RPCHandler>);

#ifdef AGRPC_STDEXEC
    template <class Receiver>
    friend auto tag_invoke(stdexec::connect_t, RPCHandlerSender&& s, Receiver&& r) noexcept(
        noexcept(static_cast<RPCHandlerSender&&>(s).connect(static_cast<Receiver&&>(r))))
    {
        return static_cast<RPCHandlerSender&&>(s).connect(static_cast<Receiver&&>(r));
    }
#endif

  private:
    template <class, class, class>
    friend struct detail::RegisterRPCHandlerSenderOperationBase;

    agrpc::GrpcContext& grpc_context_;
    Service& service_;
    RPCHandler rpc_handler_;
};

template <class ServerRPC, class RPCHandler, class StopToken>
struct RegisterRPCHandlerSenderOperationBase : RegisterRPCHandlerOperationBase<ServerRPC, RPCHandler, StopToken>,
                                               RegisterRPCHandlerOperationComplete
{
    RegisterRPCHandlerSenderOperationBase(RPCHandlerSender<ServerRPC, RPCHandler>&& sender,
                                          RegisterRPCHandlerOperationComplete::Complete complete)
        : RegisterRPCHandlerOperationBase<ServerRPC, RPCHandler, StopToken>{sender.grpc_context_.get_executor(),
                                                                            sender.service_,
                                                                            static_cast<RPCHandler&&>(
                                                                                sender.rpc_handler_)},
          RegisterRPCHandlerOperationComplete{complete}
    {
    }
};

template <class Receiver, class Signature, bool IsNotifyWhenDone>
struct GetWaitForDoneOperationState
{
    using Type =
        detail::InplaceWithFunctionWrapper<exec::connect_result_t<ManualResetEventSender<Signature>, Receiver>>;
};

template <class Receiver, class Signature>
struct GetWaitForDoneOperationState<Receiver, Signature, false>
{
    using Type = detail::Empty;
};

template <class Receiver, class Signature, bool IsNotifyWhenDone>
using GetWaitForDoneOperationStateT =
    typename GetWaitForDoneOperationState<Receiver, Signature, IsNotifyWhenDone>::Type;

struct RPCHandlerOperationFinish
{
    template <class Operation>
    static void perform(Operation& op, std::exception_ptr* eptr) noexcept
    {
        auto& rpc = op.rpc_;
        if (eptr)
        {
            op.base().set_error(static_cast<std::exception_ptr&&>(*eptr));
        }
        if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
        {
            rpc.cancel();
        }
        if constexpr (Operation::Traits::NOTIFY_WHEN_DONE)
        {
            if (!rpc.is_done())
            {
                op.start_wait_for_done();
                return;
            }
        }
        [[maybe_unused]] detail::AllocationGuard g{op, op.get_allocator()};
    }
};

struct RPCHandlerOperationWaitForDone
{
    template <class Operation>
    static void perform(Operation& op, const std::exception_ptr*) noexcept
    {
        [[maybe_unused]] detail::AllocationGuard g{op, op.get_allocator()};
    }
};

template <class ServerRPC, class RPCHandler, class StopToken, class Allocator>
void create_and_start_rpc_handler_operation(
    RegisterRPCHandlerSenderOperationBase<ServerRPC, RPCHandler, StopToken>& operation, const Allocator& allocator);

template <class ServerRPC, class RPCHandler, class StopToken, class Allocator>
struct RPCHandlerOperation
{
    using Service = detail::GetServerRPCServiceT<ServerRPC>;
    using Traits = typename ServerRPC::Traits;
    using Starter = detail::ServerRPCStarterT<ServerRPC>;
    using RPCHandlerInvokeResult = detail::RPCHandlerInvokeResultT<Starter&, RPCHandler&, ServerRPC&>;
    using RegisterRPCHandlerSenderOperationBase =
        detail::RegisterRPCHandlerSenderOperationBase<ServerRPC, RPCHandler, StopToken>;

    struct StartReceiver
    {
        using is_receiver = void;

        RPCHandlerOperation& rpc_handler_op_;

        static constexpr void set_done() noexcept {}

        void set_value(bool ok) const noexcept
        {
            auto& op = rpc_handler_op_;
            detail::AllocationGuard guard{op, op.get_allocator()};
            if (ok)
            {
                if (auto exception_ptr = op.emplace_rpc_handler_operation_state())
                {
                    op.rpc_.cancel();
                    op.base().set_error(static_cast<std::exception_ptr&&>(*exception_ptr));
                    return;
                }
                detail::create_and_start_rpc_handler_operation(op.base(), op.get_allocator());
                op.start_rpc_handler_operation_state();
                guard.release();
            }
        }

        static void set_error(const std::exception_ptr&) noexcept {}

#ifdef AGRPC_STDEXEC
        friend void tag_invoke(stdexec::set_stopped_t, const StartReceiver&) noexcept {}

        friend void tag_invoke(stdexec::set_value_t, const StartReceiver& r, bool ok) noexcept { r.set_value(ok); }

        friend void tag_invoke(stdexec::set_error_t, const StartReceiver&, const std::exception_ptr&) noexcept {}
#endif
    };

    using StartOperationState = detail::InplaceWithFunctionWrapper<
        exec::connect_result_t<decltype(std::declval<Starter>().start(std::declval<ServerRPC&>(),
                                                                      std::declval<Service&>(), agrpc::use_sender)),
                               StartReceiver>>;

    template <class Action>
    struct Receiver
    {
        using is_receiver = void;

        RPCHandlerOperation& op_;

        void perform(std::exception_ptr* eptr) const noexcept { Action::perform(op_, eptr); }

        void set_done() const noexcept { perform(nullptr); }

        template <class... T>
        void set_value(T&&...) const noexcept
        {
            perform(nullptr);
        }

        void set_error(std::exception_ptr eptr) const noexcept { perform(&eptr); }

#ifdef AGRPC_STDEXEC
        friend constexpr void tag_invoke(stdexec::set_stopped_t, const Receiver& r) noexcept { r.set_done(); }

        template <class... T>
        friend constexpr void tag_invoke(stdexec::set_value_t, const Receiver& r, T&&...) noexcept
        {
            r.set_value();
        }

        friend void tag_invoke(stdexec::set_error_t, const Receiver& r, std::exception_ptr e) noexcept
        {
            r.set_error(static_cast<std::exception_ptr&&>(e));
        }

        friend InlineSchedulerEnv tag_invoke(stdexec::get_env_t, const Receiver&) noexcept { return {}; }
#endif

        friend constexpr exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>, const Receiver&) noexcept
        {
            return {};
        }
    };

    using FinishReceiver = Receiver<RPCHandlerOperationFinish>;
    using FinishOperationState =
        detail::InplaceWithFunctionWrapper<exec::connect_result_t<RPCHandlerInvokeResult, FinishReceiver>>;

    using WaitForDoneReceiver = Receiver<RPCHandlerOperationWaitForDone>;
    using WaitForDoneOperationState =
        detail::GetWaitForDoneOperationStateT<WaitForDoneReceiver, void(), ServerRPC::Traits::NOTIFY_WHEN_DONE>;

    using OperationState = std::variant<StartOperationState, FinishOperationState, WaitForDoneOperationState>;

    explicit RPCHandlerOperation(RegisterRPCHandlerSenderOperationBase& operation, const Allocator& allocator)
        : impl1_(operation),
          rpc_(detail::ServerRPCContextBaseAccess::construct<ServerRPC>(operation.get_executor())),
          impl2_(detail::SecondThenVariadic{}, allocator, std::in_place_type<StartOperationState>,
                 detail::InplaceWithFunction{},
                 [&]
                 {
                     return initial_request()
                         .start(rpc_, operation.service(), agrpc::use_sender)
                         .connect(StartReceiver{*this});
                 })
    {
        base().increment_ref_count();
    }

    RPCHandlerOperation(const RPCHandlerOperation& other) = delete;
    RPCHandlerOperation(RPCHandlerOperation&& other) = delete;

    ~RPCHandlerOperation() noexcept
    {
        if (base().decrement_ref_count())
        {
            base().complete();
        }
    }

    RPCHandlerOperation& operator=(const RPCHandlerOperation& other) = delete;
    RPCHandlerOperation& operator=(RPCHandlerOperation&& other) = delete;

    void start() { std::get<StartOperationState>(operation_state()).value_.start(); }

#ifdef AGRPC_STDEXEC
    friend void tag_invoke(stdexec::start_t, RPCHandlerOperation& o) noexcept { o.start(); }
#endif

    std::optional<std::exception_ptr> emplace_rpc_handler_operation_state() noexcept
    {
        AGRPC_TRY
        {
            operation_state().template emplace<FinishOperationState>(
                detail::InplaceWithFunction{},
                [&]
                {
                    return exec::connect(initial_request().invoke(rpc_handler(), rpc_), FinishReceiver{*this});
                });
            return {};
        }
        AGRPC_CATCH(...) { return std::current_exception(); }
    }

    void start_rpc_handler_operation_state() noexcept
    {
        exec::start(std::get<FinishOperationState>(operation_state()).value_);
    }

    void start_wait_for_done() noexcept
    {
        auto& state = operation_state().template emplace<WaitForDoneOperationState>(
            detail::InplaceWithFunction{},
            [&]
            {
                return rpc_.wait_for_done(agrpc::use_sender).connect(WaitForDoneReceiver{*this});
            });
        state.value_.start();
    }

    auto& base() noexcept { return impl1_.first(); }

    auto& rpc_handler() noexcept { return base().rpc_handler(); }

    auto& initial_request() noexcept { return impl1_.second(); }

    auto& operation_state() noexcept { return impl2_.first(); }

    auto& get_allocator() noexcept { return impl2_.second(); }

    detail::CompressedPair<RegisterRPCHandlerSenderOperationBase&, Starter> impl1_;
    ServerRPC rpc_;
    detail::CompressedPair<OperationState, Allocator> impl2_;
};

template <class ServerRPC, class RPCHandler, class StopToken, class Allocator>
void create_and_start_rpc_handler_operation(
    RegisterRPCHandlerSenderOperationBase<ServerRPC, RPCHandler, StopToken>& operation, const Allocator& allocator)
{
    if AGRPC_UNLIKELY (operation.is_stopped())
    {
        return;
    }
    using RPCHandlerOperation = detail::RPCHandlerOperation<ServerRPC, RPCHandler, StopToken, Allocator>;
    auto rpc_handler_operation_guard = detail::allocate<RPCHandlerOperation>(allocator, operation, allocator);
    (*rpc_handler_operation_guard).start();
    rpc_handler_operation_guard.release();
}

template <class ServerRPC, class RPCHandler, class Receiver>
class RPCHandlerSenderOperation
    : public detail::RegisterRPCHandlerSenderOperationBase<ServerRPC, RPCHandler, exec::stop_token_type_t<Receiver&>>
{
  private:
    using StopToken = exec::stop_token_type_t<Receiver&>;
    using Base = detail::RegisterRPCHandlerSenderOperationBase<ServerRPC, RPCHandler, StopToken>;
    using Allocator = detail::RemoveCrefT<decltype(exec::get_allocator(std::declval<Receiver&>()))>;
    using RPCHandlerOperation = detail::RPCHandlerOperation<ServerRPC, RPCHandler, StopToken, Allocator>;
    using RPCHandlerSender = detail::RPCHandlerSender<ServerRPC, RPCHandler>;

    friend RPCHandlerOperation;

  public:
    void start() noexcept
    {
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(this->grpc_context()))
        {
            exec::set_done(static_cast<Receiver&&>(receiver_));
            return;
        }
        auto stop_token = exec::get_stop_token(receiver_);
        if (stop_token.stop_requested())
        {
            exec::set_done(static_cast<Receiver&&>(receiver_));
            return;
        }
        this->stop_context_.emplace(std::move(stop_token));
        detail::create_and_start_rpc_handler_operation(*this, get_allocator());
    }

#ifdef AGRPC_STDEXEC
    friend void tag_invoke(stdexec::start_t, RPCHandlerSenderOperation& o) noexcept { o.start(); }
#endif

  private:
    friend RPCHandlerSender;

    template <class R>
    RPCHandlerSenderOperation(RPCHandlerSender&& sender, R&& receiver)
        : Base(static_cast<RPCHandlerSender&&>(sender), &complete_impl), receiver_(static_cast<R&&>(receiver))
    {
    }

    static void complete_impl(RegisterRPCHandlerOperationComplete& operation) noexcept
    {
        auto& self = static_cast<RPCHandlerSenderOperation&>(operation);
        self.stop_context_.reset();
        if (self.eptr_)
        {
            exec::set_error(static_cast<Receiver&&>(self.receiver_), static_cast<std::exception_ptr&&>(self.eptr_));
        }
        else
        {
            exec::set_done(static_cast<Receiver&&>(self.receiver_));
        }
    }

    decltype(auto) get_allocator() noexcept { return exec::get_allocator(receiver_); }

    Receiver receiver_;
};

template <class ServerRPC, class RPCHandler>
template <class Receiver>
inline auto RPCHandlerSender<ServerRPC, RPCHandler>::connect(Receiver&& receiver) && noexcept(
    detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> && std::is_nothrow_move_constructible_v<RPCHandler>)
{
    return RPCHandlerSenderOperation<ServerRPC, RPCHandler, detail::RemoveCrefT<Receiver>>{
        static_cast<RPCHandlerSender&&>(*this), static_cast<Receiver&&>(receiver)};
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_SENDER_RPC_HANDLER_HPP
