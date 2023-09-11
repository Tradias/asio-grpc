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

#ifndef AGRPC_DETAIL_REQUEST_HANDLER_SENDER_HPP
#define AGRPC_DETAIL_REQUEST_HANDLER_SENDER_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/atomic_bool_stop_context.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <atomic>
#include <optional>
#include <variant>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ServerRPC, class RequestHandler, class StopToken, class Allocator>
struct RequestHandlerSenderOperationBase;

template <class ServerRPC, class RequestHandler>
class RequestHandlerSender : public detail::SenderOf<void()>
{
  private:
    using Service = detail::GetServerRPCServiceT<ServerRPC>;

  public:
    RequestHandlerSender(agrpc::GrpcContext& grpc_context, Service& service, RequestHandler&& request_handler)
        : grpc_context_(grpc_context),
          service_(service),
          request_handler_(static_cast<RequestHandler&&>(request_handler))
    {
    }

    template <class Receiver>
    auto connect(Receiver&& receiver) && noexcept(
        detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_move_constructible_v<RequestHandler>);

  private:
    template <class, class, class, class>
    friend struct detail::RequestHandlerSenderOperationBase;

    agrpc::GrpcContext& grpc_context_;
    Service& service_;
    RequestHandler request_handler_;
};

class RequestHandlerSenderOperationComplete
{
  public:
    using Complete = void (*)(RequestHandlerSenderOperationComplete&) noexcept;

    RequestHandlerSenderOperationComplete(Complete complete) noexcept : complete_(complete) {}

    void complete() noexcept { complete_(*this); }

  private:
    Complete complete_;
};

template <class ServerRPC, class RequestHandler, class StopToken, class Allocator>
struct RequestHandlerSenderOperationBase : RequestHandlerSenderOperationComplete
{
    using Service = detail::GetServerRPCServiceT<ServerRPC>;
    using Sender = RequestHandlerSender<ServerRPC, RequestHandler>;

    RequestHandlerSenderOperationBase(Sender&& sender, RequestHandlerSenderOperationComplete::Complete complete)
        : RequestHandlerSenderOperationComplete{complete}, sender_(static_cast<Sender&&>(sender))
    {
    }

    bool is_stopped() const noexcept { return stop_context_.is_stopped(); }

    void stop() noexcept { stop_context_.stop(); }

    void create_and_start_request_handler_operation(const Allocator& allocator);

    agrpc::GrpcContext& grpc_context() const noexcept { return sender_.grpc_context_; }

    Service& service() const noexcept { return sender_.service_; }

    const RequestHandler& request_handler() const noexcept { return sender_.request_handler_; }

    void set_error(std::exception_ptr&& eptr) noexcept
    {
        if (!has_error_.exchange(true))
        {
            eptr_ = static_cast<std::exception_ptr&&>(eptr);
        }
    }

    void increment_ref_count() noexcept { ++reference_count_; }

    [[nodiscard]] bool decrement_ref_count() noexcept { return 0 == --reference_count_; }

    Sender sender_;
    std::atomic_size_t reference_count_{};
    std::exception_ptr eptr_;
    detail::AtomicBoolStopContext<StopToken> stop_context_;
    std::atomic_bool has_error_{};
};

template <class Receiver, class Signature, bool IsSet>
struct WaitForOperationState
{
    using Type =
        detail::InplaceWithFunctionWrapper<exec::connect_result_t<ManualResetEventSender<Signature>, Receiver>>;
};

template <class Receiver, class Signature>
struct WaitForOperationState<Receiver, Signature, false>
{
    using Type = detail::Empty;
};

template <class Receiver, class Signature, bool IsSet>
using WaitForOperationStateT = typename WaitForOperationState<Receiver, Signature, IsSet>::Type;

struct RequestHandlerOperationFinish
{
    template <class Operation>
    static void perform(Operation& op, std::exception_ptr* eptr)
    {
        auto& rpc = op.rpc_;
        if (eptr)
        {
            op.base_.stop();
            op.base_.set_error(static_cast<std::exception_ptr&&>(*eptr));
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
        if constexpr (Operation::Traits::RESUMABLE_READ)
        {
            if (detail::ServerRPCReadMixinAccess::is_reading(rpc))
            {
                op.start_wait_for_read();
                return;
            }
        }
        detail::destroy_deallocate(&op, op.get_allocator());
    }
};

struct RequestHandlerOperationWaitForDone
{
    template <class Operation>
    static void perform(Operation& op, std::exception_ptr*)
    {
        auto& rpc = op.rpc_;
        if constexpr (Operation::Traits::RESUMABLE_READ)
        {
            if (detail::ServerRPCReadMixinAccess::is_reading(rpc))
            {
                op.start_wait_for_read();
                return;
            }
        }
        detail::destroy_deallocate(&op, op.get_allocator());
    }
};

struct RequestHandlerOperationWaitForRead
{
    template <class Operation>
    static void perform(Operation& op, std::exception_ptr*)
    {
        detail::destroy_deallocate(&op, op.get_allocator());
    }
};

template <class ServerRPC, class RequestHandler, class StopToken, class Allocator>
struct RequestHandlerOperation
{
    using Service = detail::GetServerRPCServiceT<ServerRPC>;
    using Traits = typename ServerRPC::Traits;
    using InitialRequest =
        detail::RPCRequest<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>;
    using RequestHandlerInvokeResult =
        decltype(std::declval<InitialRequest>().invoke(std::declval<RequestHandler>(), std::declval<ServerRPC&>()));
    using RequestHandlerSenderOperationBase =
        detail::RequestHandlerSenderOperationBase<ServerRPC, RequestHandler, StopToken, Allocator>;

    static_assert(exec::is_sender_v<RequestHandlerInvokeResult>, "Request handler must return a sender.");

    struct StartReceiver
    {
        RequestHandlerOperation& request_handler_op_;

        static constexpr void set_done() noexcept {}

        void set_value(bool ok)
        {
            auto& base = request_handler_op_.base_;
            detail::AllocationGuard ptr{&request_handler_op_, request_handler_op_.get_allocator()};
            if (ok)
            {
                if (auto exception_ptr = request_handler_op_.emplace_request_handler_operation_state())
                {
                    request_handler_op_.rpc_.cancel();
                    base.set_error(static_cast<std::exception_ptr&&>(*exception_ptr));
                    return;
                }
                base.create_and_start_request_handler_operation(request_handler_op_.get_allocator());
                request_handler_op_.start_request_handler_operation_state();
                ptr.release();
            }
        }

        static constexpr void set_error(const std::exception_ptr&) noexcept {}

        friend exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>, const StartReceiver&) noexcept
        {
            return {};
        }
    };

    using StartOperationState = detail::InplaceWithFunctionWrapper<
        exec::connect_result_t<decltype(std::declval<InitialRequest>().start(
                                   std::declval<ServerRPC&>(), std::declval<Service&>(), agrpc::use_sender)),
                               StartReceiver>>;

    template <class Action>
    struct Receiver
    {
        RequestHandlerOperation& op_;

        void perform(std::exception_ptr* eptr) noexcept { Action::perform(op_, eptr); }

        void set_done() noexcept { perform(nullptr); }

        template <class... T>
        void set_value(T&&...) noexcept
        {
            perform(nullptr);
        }

        void set_error(std::exception_ptr eptr) noexcept { perform(&eptr); }

        friend exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>, const Receiver&) noexcept
        {
            return {};
        }
    };

    using FinishReceiver = Receiver<RequestHandlerOperationFinish>;
    using FinishOperationState =
        detail::InplaceWithFunctionWrapper<exec::connect_result_t<RequestHandlerInvokeResult, FinishReceiver>>;

    using WaitForDoneReceiver = Receiver<RequestHandlerOperationWaitForDone>;
    using WaitForDoneOperationState =
        detail::WaitForOperationStateT<WaitForDoneReceiver, void(), ServerRPC::Traits::NOTIFY_WHEN_DONE>;

    using WaitForReadReceiver = Receiver<RequestHandlerOperationWaitForRead>;
    using WaitForReadOperationState =
        detail::WaitForOperationStateT<WaitForReadReceiver, void(bool), ServerRPC::Traits::RESUMABLE_READ>;

    using OperationState =
        std::variant<StartOperationState, FinishOperationState, WaitForDoneOperationState, WaitForReadOperationState>;

    explicit RequestHandlerOperation(RequestHandlerSenderOperationBase& operation, const Allocator& allocator)
        : base_(operation),
          impl1_(operation.request_handler()),
          rpc_(detail::ServerRPCContextBaseAccess::construct<ServerRPC>(operation.grpc_context().get_executor())),
          impl2_(detail::SecondThenVariadic{}, allocator, std::in_place_type<StartOperationState>,
                 detail::InplaceWithFunction{},
                 [&]
                 {
                     return initial_request()
                         .start(rpc_, operation.service(), agrpc::use_sender)
                         .connect(StartReceiver{*this});
                 })
    {
        base_.increment_ref_count();
    }

    RequestHandlerOperation(const RequestHandlerOperation& other) = delete;
    RequestHandlerOperation(RequestHandlerOperation&& other) = delete;

    ~RequestHandlerOperation() noexcept
    {
        if (base_.decrement_ref_count())
        {
            base_.complete();
        }
    }

    RequestHandlerOperation& operator=(const RequestHandlerOperation& other) = delete;
    RequestHandlerOperation& operator=(RequestHandlerOperation&& other) = delete;

    void start() { std::get<StartOperationState>(operation_state()).value_.start(); }

    std::optional<std::exception_ptr> emplace_request_handler_operation_state()
    {
        AGRPC_TRY
        {
            operation_state().template emplace<FinishOperationState>(
                detail::InplaceWithFunction{},
                [&]
                {
                    return exec::connect(
                        initial_request().invoke(static_cast<RequestHandler&&>(request_handler()), rpc_),
                        FinishReceiver{*this});
                });
            return {};
        }
        AGRPC_CATCH(...) { return std::current_exception(); }
    }

    void start_request_handler_operation_state()
    {
        exec::start(std::get<FinishOperationState>(operation_state()).value_);
    }

    void start_wait_for_done()
    {
        auto& state = operation_state().template emplace<WaitForDoneOperationState>(
            detail::InplaceWithFunction{},
            [&]
            {
                return rpc_.wait_for_done(agrpc::use_sender).connect(WaitForDoneReceiver{*this});
            });
        state.value_.start();
    }

    void start_wait_for_read()
    {
        auto& state = operation_state().template emplace<WaitForReadOperationState>(
            detail::InplaceWithFunction{},
            [&]
            {
                return rpc_.wait_for_read(agrpc::use_sender).connect(WaitForReadReceiver{*this});
            });
        state.value_.start();
    }

    auto& request_handler() noexcept { return impl1_.first(); }

    auto& initial_request() noexcept { return impl1_.second(); }

    auto& operation_state() noexcept { return impl2_.first(); }

    auto& get_allocator() noexcept { return impl2_.second(); }

    RequestHandlerSenderOperationBase& base_;
    detail::CompressedPair<RequestHandler, InitialRequest> impl1_;
    ServerRPC rpc_;
    detail::CompressedPair<OperationState, Allocator> impl2_;
};

template <class ServerRPC, class RequestHandler, class StopToken, class Allocator>
inline void
RequestHandlerSenderOperationBase<ServerRPC, RequestHandler, StopToken,
                                  Allocator>::create_and_start_request_handler_operation(const Allocator& allocator)
{
    if AGRPC_UNLIKELY (is_stopped())
    {
        return;
    }
    using RequestHandlerOperation = detail::RequestHandlerOperation<ServerRPC, RequestHandler, StopToken, Allocator>;
    auto request_handler_operation = detail::allocate<RequestHandlerOperation>(allocator, *this, allocator);
    request_handler_operation->start();
    request_handler_operation.release();
    return;
}

template <class ServerRPC, class RequestHandler, class Receiver>
class RequestHandlerSenderOperation : public detail::RequestHandlerSenderOperationBase<
                                          ServerRPC, RequestHandler, exec::stop_token_type_t<Receiver&>,
                                          detail::RemoveCrefT<decltype(exec::get_allocator(std::declval<Receiver&>()))>>
{
  private:
    using StopToken = exec::stop_token_type_t<Receiver&>;
    using Allocator = detail::RemoveCrefT<decltype(exec::get_allocator(std::declval<Receiver&>()))>;
    using Base = detail::RequestHandlerSenderOperationBase<ServerRPC, RequestHandler, StopToken, Allocator>;
    using RequestHandlerOperation = detail::RequestHandlerOperation<ServerRPC, RequestHandler, StopToken, Allocator>;
    using RequestHandlerSender = detail::RequestHandlerSender<ServerRPC, RequestHandler>;

    friend RequestHandlerOperation;

  public:
    void start() noexcept
    {
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(this->grpc_context()))
        {
            exec::set_done(static_cast<Receiver&&>(receiver_));
            return;
        }
        auto stop_token = exec::get_stop_token(receiver_);
        if (detail::stop_requested(stop_token))
        {
            exec::set_done(static_cast<Receiver&&>(receiver_));
            return;
        }
        this->stop_context_.emplace(std::move(stop_token));
        this->create_and_start_request_handler_operation(get_allocator());
    }

  private:
    friend RequestHandlerSender;

    template <class R>
    RequestHandlerSenderOperation(RequestHandlerSender&& sender, R&& receiver)
        : Base(static_cast<RequestHandlerSender&&>(sender), &complete_impl), receiver_(static_cast<R&&>(receiver))
    {
    }

    static void complete_impl(RequestHandlerSenderOperationComplete& operation) noexcept
    {
        auto& self = static_cast<RequestHandlerSenderOperation&>(operation);
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

template <class ServerRPC, class RequestHandler>
template <class Receiver>
inline auto RequestHandlerSender<ServerRPC, RequestHandler>::connect(Receiver&& receiver) && noexcept(
    detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_move_constructible_v<RequestHandler>)
{
    return RequestHandlerSenderOperation<ServerRPC, RequestHandler, detail::RemoveCrefT<Receiver>>{
        static_cast<RequestHandlerSender&&>(*this), static_cast<Receiver&&>(receiver)};
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REQUEST_HANDLER_SENDER_HPP
