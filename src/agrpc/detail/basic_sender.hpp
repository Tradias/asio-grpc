// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_BASIC_SENDER_HPP
#define AGRPC_DETAIL_BASIC_SENDER_HPP

#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_implementation.hpp>
#include <agrpc/detail/operation_initiation.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Initiation, class Implementation, class Receiver>
class BasicSenderOperationState;

#ifdef AGRPC_STDEXEC
struct BasicSenderEnv
{
    template <class Tag>
    friend agrpc::GrpcContext::executor_type tag_invoke(stdexec::get_completion_scheduler_t<Tag>,
                                                        const BasicSenderEnv& e) noexcept;

    friend constexpr exec::inline_scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_stopped_t>,
                                                       const BasicSenderEnv&) noexcept
    {
        return {};
    }

    agrpc::GrpcContext& grpc_context_;
};
#endif

template <class Initiation, class Implementation>
class [[nodiscard]] BasicSender : public detail::SenderOf<typename Implementation::Signature>
{
  public:
    template <class Receiver>
    [[nodiscard]] detail::BasicSenderOperationState<Initiation, Implementation, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) && noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                          std::is_nothrow_copy_constructible_v<Initiation> &&
                                          std::is_nothrow_move_constructible_v<Implementation>))
    {
        return {static_cast<Receiver&&>(receiver), grpc_context_, initiation_,
                static_cast<Implementation&&>(implementation_)};
    }

    template <class Receiver, class Impl = Implementation, class = std::enable_if_t<std::is_copy_constructible_v<Impl>>>
    [[nodiscard]] detail::BasicSenderOperationState<Initiation, Implementation, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) const& noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                              std::is_nothrow_copy_constructible_v<Initiation> &&
                                              std::is_nothrow_copy_constructible_v<Implementation>))
    {
        return {static_cast<Receiver&&>(receiver), grpc_context_, initiation_, implementation_};
    }

#ifdef AGRPC_STDEXEC
    template <class Receiver>
    friend detail::BasicSenderOperationState<Initiation, Implementation, detail::RemoveCrefT<Receiver>> tag_invoke(
        stdexec::connect_t, BasicSender&& s,
        Receiver&& r) noexcept(noexcept(static_cast<BasicSender&&>(s).connect(static_cast<Receiver&&>(r))))
    {
        return static_cast<BasicSender&&>(s).connect(static_cast<Receiver&&>(r));
    }

    template <class Receiver>
    friend detail::BasicSenderOperationState<Initiation, Implementation, detail::RemoveCrefT<Receiver>> tag_invoke(
        stdexec::connect_t, const BasicSender& s,
        Receiver&& r) noexcept(noexcept(s.connect(static_cast<Receiver&&>(r))))
    {
        return s.connect(static_cast<Receiver&&>(r));
    }

    friend BasicSenderEnv tag_invoke(stdexec::get_env_t, const BasicSender& s) noexcept { return {s.grpc_context_}; }
#endif

  private:
    friend detail::BasicSenderAccess;

    template <class, class, class>
    friend class detail::BasicSenderOperationState;

    BasicSender(agrpc::GrpcContext& grpc_context, const Initiation& initiation, Implementation&& implementation)
        : grpc_context_(grpc_context),
          initiation_(initiation),
          implementation_{static_cast<Implementation&&>(implementation)}
    {
    }

    agrpc::GrpcContext& grpc_context_;
    Initiation initiation_;
    Implementation implementation_;
};

struct BasicSenderAccess
{
    template <class Initiation, class Implementation>
    static auto create(agrpc::GrpcContext& grpc_context, const Initiation& initiation, Implementation&& implementation)
    {
        return detail::BasicSender<Initiation, detail::RemoveCrefT<Implementation>>(
            grpc_context, initiation, static_cast<Implementation&&>(implementation));
    }
};

template <class ImplementationT, class Receiver>
class BasicSenderRunningOperation : public ImplementationT::BaseType
{
  public:
    using Implementation = ImplementationT;

  private:
    using Base = typename ImplementationT::BaseType;
    using StopFunction = typename ImplementationT::StopFunction;
    using StopToken = exec::stop_token_type_t<Receiver&>;
    using StopCallback = detail::StopCallbackLifetime<StopToken, StopFunction>;

    template <int Id = 0>
    static void do_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        auto& self = *static_cast<BasicSenderRunningOperation*>(op);
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            detail::complete<AllocationType::NONE, Id>(self, result, grpc_context);
        }
        else
        {
            exec::set_done(static_cast<Receiver&&>(self.receiver()));
        }
    }

    template <class Initiation>
    void emplace_stop_callback(StopToken&& stop_token, const Initiation& initiation) noexcept
    {
        if constexpr (StopCallback::IS_STOPPABLE)
        {
            stop_callback().emplace(static_cast<StopToken&&>(stop_token),
                                    detail::get_stop_function_arg(initiation, implementation()));
        }
    }

    void reset_stop_callback() noexcept { stop_callback().reset(); }

  public:
    template <class R>
    BasicSenderRunningOperation(R&& receiver, ImplementationT&& implementation)
        : Base(&do_complete), impl_(static_cast<R&&>(receiver), static_cast<ImplementationT&&>(implementation))
    {
    }

    template <class R>
    BasicSenderRunningOperation(R&& receiver, const ImplementationT& implementation)
        : Base(&do_complete), impl_(static_cast<R&&>(receiver), implementation)
    {
    }

    template <class Initiation>
    void start(agrpc::GrpcContext& grpc_context, const Initiation& initiation, StopToken&& stop_token) noexcept
    {
        grpc_context.work_started();
        emplace_stop_callback(static_cast<StopToken&&>(stop_token), initiation);
        detail::initiate<detail::DeallocateOnComplete::NO_>(*this, grpc_context, initiation, AllocationType::NONE);
    }

    Receiver& receiver() noexcept { return impl_.first().first(); }

    StopCallback& stop_callback() noexcept { return impl_.first().second(); }

    ImplementationT& implementation() noexcept { return impl_.second(); }

    Base* tag() noexcept { return this; }

    template <AllocationType, int Id>
    void set_on_complete() noexcept
    {
        detail::OperationBaseAccess::set_on_complete(*this, &do_complete<Id>);
    }

    template <AllocationType, class... Args>
    void complete(Args... args) noexcept
    {
        reset_stop_callback();
        exec::set_value(static_cast<Receiver&&>(receiver()), static_cast<Args&&>(args)...);
    }

    void done() noexcept
    {
        reset_stop_callback();
        exec::set_done(static_cast<Receiver&&>(receiver()));
    }

    void put_into_scratch_space(void* ptr) noexcept { detail::OperationBaseAccess::set_scratch_space(*this, ptr); }

    [[nodiscard]] void* get_scratch_space() const noexcept
    {
        return detail::OperationBaseAccess::get_scratch_space(*this);
    }

    void restore_scratch_space() noexcept { detail::OperationBaseAccess::set_on_complete(*this, &do_complete); }

  private:
    detail::CompressedPair<detail::CompressedPair<Receiver, StopCallback>, ImplementationT> impl_;
};

template <class Initiation, class Implementation, class Receiver>
class BasicSenderOperationState
{
  public:
    void start() noexcept
    {
        auto& op = operation();
        auto* const scratch_space = op.get_scratch_space();
        auto& grpc_context = *static_cast<agrpc::GrpcContext*>(scratch_space);
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
        {
            exec::set_done(static_cast<Receiver&&>(receiver()));
            return;
        }
        auto stop_token = exec::get_stop_token(receiver());
        if (stop_token.stop_requested())
        {
            exec::set_done(static_cast<Receiver&&>(receiver()));
            return;
        }
        op.restore_scratch_space();
        op.start(grpc_context, impl_.second(), std::move(stop_token));
    }

#ifdef AGRPC_STDEXEC
    friend void tag_invoke(stdexec::start_t, BasicSenderOperationState& s) noexcept { s.start(); }
#endif

  private:
    friend detail::BasicSender<Initiation, Implementation>;

    template <class R, class Impl>
    BasicSenderOperationState(R&& receiver, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                              Impl&& implementation)
        : impl_(detail::SecondThenVariadic{}, initiation, static_cast<R&&>(receiver),
                static_cast<Impl&&>(implementation))
    {
        operation().put_into_scratch_space(&grpc_context);
    }

    auto& operation() noexcept { return impl_.first(); }

    Receiver& receiver() noexcept { return operation().receiver(); }

    detail::CompressedPair<detail::BasicSenderRunningOperation<Implementation, Receiver>, Initiation> impl_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_BASIC_SENDER_HPP
