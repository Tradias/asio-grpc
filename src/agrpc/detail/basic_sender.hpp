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

#ifndef AGRPC_DETAIL_BASIC_SENDER_HPP
#define AGRPC_DETAIL_BASIC_SENDER_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_implementation.hpp>
#include <agrpc/detail/operation_initiation.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Initiation, class Implementation, class Receiver>
class BasicSenderOperationState;

template <class Initiation, class Implementation>
class BasicSender : public detail::SenderOf<typename Implementation::Signature>
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
class BasicSenderRunningOperation : public detail::BaseForSenderImplementationTypeT<ImplementationT::TYPE>
{
  public:
    using Implementation = ImplementationT;

  private:
    using Base = detail::BaseForSenderImplementationTypeT<Implementation::TYPE>;
    using StopFunction = typename Implementation::StopFunction;
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
    BasicSenderRunningOperation(R&& receiver, Implementation&& implementation)
        : Base(&do_complete), impl_(static_cast<R&&>(receiver), static_cast<Implementation&&>(implementation))
    {
    }

    template <class R>
    BasicSenderRunningOperation(R&& receiver, const Implementation& implementation)
        : Base(&do_complete), impl_(static_cast<R&&>(receiver), implementation)
    {
    }

    template <class Initiation>
    void start(agrpc::GrpcContext& grpc_context, const Initiation& initiation, StopToken&& stop_token) noexcept
    {
        grpc_context.work_started();
        emplace_stop_callback(static_cast<StopToken&&>(stop_token), initiation);
        detail::initiate<detail::DeallocateOnComplete::NO>(*this, grpc_context, initiation, AllocationType::NONE);
    }

    Receiver& receiver() noexcept { return impl_.first().first(); }

    StopCallback& stop_callback() noexcept { return impl_.first().second(); }

    Implementation& implementation() noexcept { return impl_.second(); }

    Base* tag() noexcept { return this; }

    template <AllocationType, int Id>
    void set_on_complete() noexcept
    {
        detail::OperationBaseAccess::set_on_complete(*this, &do_complete<Id>);
    }

    template <AllocationType, class... Args>
    void complete(agrpc::GrpcContext&, Args... args)
    {
        reset_stop_callback();
        detail::satisfy_receiver(static_cast<Receiver&&>(receiver()), static_cast<Args&&>(args)...);
    }

    void put_into_scratch_space(void* ptr) noexcept { detail::OperationBaseAccess::set_scratch_space(*this, ptr); }

    [[nodiscard]] void* get_scratch_space() const noexcept
    {
        return detail::OperationBaseAccess::get_scratch_space(*this);
    }

    void restore_scratch_space() noexcept { detail::OperationBaseAccess::set_on_complete(*this, &do_complete); }

  private:
    detail::CompressedPair<detail::CompressedPair<Receiver, StopCallback>, Implementation> impl_;
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
        if (detail::stop_requested(stop_token))
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

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
#if !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT) && !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)
template <class Initiation, class Implementation, class R>
struct agrpc::asio::traits::connect_member<agrpc::detail::BasicSender<Initiation, Implementation>, R>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept =
        noexcept(std::declval<agrpc::detail::BasicSender<Initiation, Implementation>>().connect(std::declval<R>()));

    using result_type =
        decltype(std::declval<agrpc::detail::BasicSender<Initiation, Implementation>>().connect(std::declval<R>()));
};
#endif

#if !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT) && !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)
template <class Initiation, class Implementation, class Receiver>
struct agrpc::asio::traits::start_member<agrpc::detail::BasicSenderOperationState<Initiation, Implementation, Receiver>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};
#endif
#endif

#endif  // AGRPC_DETAIL_BASIC_SENDER_HPP
