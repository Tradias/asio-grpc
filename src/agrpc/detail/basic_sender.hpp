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
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver>
[[nodiscard]] std::optional<detail::exec::stop_token_type_t<Receiver&>> check_start_conditions(
    const agrpc::GrpcContext& grpc_context, Receiver& receiver)
{
    if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
    {
        detail::exec::set_done(static_cast<Receiver&&>(receiver));
        return std::nullopt;
    }
    auto stop_token = detail::exec::get_stop_token(receiver);
    if (stop_token.stop_requested())
    {
        detail::exec::set_done(static_cast<Receiver&&>(receiver));
        return std::nullopt;
    }
    return stop_token;
}

template <class Initiation, class Implementation>
class BasicSender;

struct BasicSenderAccess
{
    template <class Initiation, class Implementation>
    static auto create(agrpc::GrpcContext& grpc_context, const Initiation& initiation, Implementation&& implementation)
    {
        return detail::BasicSender<Initiation, detail::RemoveCrefT<Implementation>>(
            grpc_context, initiation, static_cast<Implementation&&>(implementation));
    }
};

template <class Implementation, class Receiver, detail::DeallocateOnComplete Deallocate>
class BasicSenderRunningOperation;

template <class Implementation>
struct BasicSenderRunningOperationTemplate
{
    template <class Receiver>
    using Type = detail::BasicSenderRunningOperation<Implementation, Receiver, detail::DeallocateOnComplete::YES>;
};

template <class Receiver, class Initiation, class Implementation>
void submit_basic_sender_running_operation(agrpc::GrpcContext& grpc_context, Receiver receiver,
                                           const Initiation& initiation, Implementation&& implementation)
{
    if (auto stop_token = detail::check_start_conditions(grpc_context, receiver))
    {
        auto operation = detail::allocate_operation<
            detail::BasicSenderRunningOperationTemplate<detail::RemoveCrefT<Implementation>>::template Type>(
            grpc_context, static_cast<Receiver&&>(receiver), static_cast<Implementation&&>(implementation));
        operation->start(grpc_context, initiation, std::move(*stop_token));
    }
}

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

    template <class Receiver>
    void submit(Receiver&& receiver) &&
    {
        detail::submit_basic_sender_running_operation(grpc_context_, static_cast<Receiver&&>(receiver), initiation_,
                                                      static_cast<Implementation&&>(implementation_));
    }

    template <class Receiver, class Impl = Implementation, class = std::enable_if_t<std::is_copy_constructible_v<Impl>>>
    void submit(Receiver&& receiver) const&
    {
        detail::submit_basic_sender_running_operation(grpc_context_, static_cast<Receiver&&>(receiver), initiation_,
                                                      implementation_);
    }

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

template <class Implementation, class Receiver, detail::DeallocateOnComplete Deallocate>
class BasicSenderRunningOperation : public detail::BaseForSenderImplementationTypeT<Implementation::TYPE>
{
  private:
    using Base = detail::BaseForSenderImplementationTypeT<Implementation::TYPE>;
    using StopFunction = typename Implementation::StopFunction;
    using StopToken = detail::exec::stop_token_type_t<Receiver&>;

    template <detail::AllocationType AllocType>
    struct OnDone
    {
        template <int Id>
        struct Type
        {
            static constexpr auto ALLOCATION_TYPE = AllocType;

            template <class... Args>
            void operator()(Args... args)
            {
                self_.reset_stop_callback();
                auto receiver = self_.extract_receiver_and_optionally_deallocate<AllocType>(grpc_context_);
                detail::satisfy_receiver(static_cast<Receiver&&>(receiver), static_cast<Args&&>(args)...);
            }

            template <int NextId = Id>
            [[nodiscard]] Base* self() const noexcept
            {
                if constexpr (NextId != Id)
                {
                    self_.set_on_complete<AllocType, NextId>();
                }
                return &self_;
            }

            [[nodiscard]] agrpc::GrpcContext& grpc_context() const noexcept { return grpc_context_; }

            BasicSenderRunningOperation& self_;
            agrpc::GrpcContext& grpc_context_;
        };
    };

    template <detail::AllocationType AllocType>
    Receiver extract_receiver_and_deallocate([[maybe_unused]] agrpc::GrpcContext& grpc_context)
    {
        Receiver local_receiver{static_cast<Receiver&&>(receiver())};
        if constexpr (AllocType == detail::AllocationType::LOCAL)
        {
            detail::destroy_deallocate(this, grpc_context.get_allocator());
        }
        else
        {
            detail::destroy_deallocate(this, detail::exec::get_allocator(local_receiver));
        }
        return local_receiver;
    }

    template <detail::AllocationType AllocType>
    Receiver extract_receiver_and_optionally_deallocate([[maybe_unused]] agrpc::GrpcContext& grpc_context)
    {
        if constexpr (AllocType == detail::AllocationType::NONE)
        {
            return static_cast<Receiver&&>(receiver());
        }
        else
        {
            return extract_receiver_and_deallocate<AllocType>(grpc_context);
        }
    }

    template <detail::AllocationType AllocType, int Id, class... Args>
    auto done(agrpc::GrpcContext& grpc_context, Args&&... args) -> decltype((void)std::declval<Implementation&>().done(
        std::declval<typename OnDone<AllocType>::template Type<Id>>(), static_cast<Args&&>(args)...))
    {
        implementation().done(typename OnDone<AllocType>::template Type<Id>{*this, grpc_context},
                              static_cast<Args&&>(args)...);
    }

    template <detail::AllocationType AllocType, int Id, class... Args>
    auto done(agrpc::GrpcContext& grpc_context, Args&&... args)
        -> decltype((void)std::declval<Implementation&>().done(grpc_context, static_cast<Args&&>(args)...))
    {
        implementation().done(grpc_context, args...);
        reset_stop_callback();
        auto receiver = extract_receiver_and_optionally_deallocate<AllocType>(grpc_context);
        detail::satisfy_receiver(static_cast<Receiver&&>(receiver), static_cast<Args&&>(args)...);
    }

    template <detail::AllocationType AllocType, int Id = 0>
    static void do_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        auto& self = *static_cast<BasicSenderRunningOperation*>(op);
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            if constexpr (Implementation::TYPE == detail::SenderImplementationType::BOTH ||
                          Implementation::TYPE == detail::SenderImplementationType::GRPC_TAG)
            {
                self.template done<AllocType, Id>(grpc_context, detail::is_ok(result));
            }
            else
            {
                self.template done<AllocType, Id>(grpc_context);
            }
        }
        else
        {
            detail::exec::set_done(self.template extract_receiver_and_optionally_deallocate<AllocType>(grpc_context));
        }
    }

    static detail::OperationOnComplete get_on_complete([[maybe_unused]] detail::AllocationType allocation_type)
    {
        if constexpr (Deallocate == detail::DeallocateOnComplete::YES)
        {
            if (allocation_type == detail::AllocationType::LOCAL)
            {
                return &do_complete<detail::AllocationType::LOCAL>;
            }
            return &do_complete<detail::AllocationType::CUSTOM>;
        }
        else
        {
            return &do_complete<detail::AllocationType::NONE>;
        }
    }

    template <detail::AllocationType AllocType, int Id>
    void set_on_complete() noexcept
    {
        detail::OperationBaseAccess::get_on_complete(*this) = &do_complete<AllocType, Id>;
    }

    template <detail::AllocationType AllocType, int Id = 0>
    [[nodiscard]] bool current_on_complete_is() const noexcept
    {
        return detail::OperationBaseAccess::get_on_complete(*this) == &do_complete<AllocType, Id>;
    }

    template <detail::AllocationType AllocType>
    struct Init
    {
        static constexpr auto ALLOCATION_TYPE = AllocType;

        template <int NextId = 0>
        [[nodiscard]] Base* self() const noexcept
        {
            if constexpr (NextId != 0)
            {
                self_.set_on_complete<AllocType, NextId>();
            }
            return &self_;
        }

        [[nodiscard]] agrpc::GrpcContext& grpc_context() const noexcept { return grpc_context_; }

        BasicSenderRunningOperation& self_;
        agrpc::GrpcContext& grpc_context_;
    };

    template <class Initiation, class... T>
    void initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation, T...)
    {
        if constexpr (Deallocate == detail::DeallocateOnComplete::YES)
        {
            if (current_on_complete_is<detail::AllocationType::LOCAL>())
            {
                initiation.initiate(Init<detail::AllocationType::LOCAL>{*this, grpc_context}, implementation());
            }
            else
            {
                initiation.initiate(Init<detail::AllocationType::CUSTOM>{*this, grpc_context}, implementation());
            }
        }
        else
        {
            initiation.initiate(Init<detail::AllocationType::NONE>{*this, grpc_context}, implementation());
        }
    }

    template <class Initiation, class Impl = Implementation>
    auto initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation)
        -> decltype((void)initiation.initiate(grpc_context, std::declval<Impl&>(), static_cast<Base*>(nullptr)))
    {
        initiation.initiate(grpc_context, implementation(), static_cast<Base*>(this));
    }

    template <class Initiation>
    auto initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation)
        -> decltype((void)initiation.initiate(grpc_context, static_cast<Base*>(nullptr)))
    {
        initiation.initiate(grpc_context, static_cast<Base*>(this));
    }

    Implementation& implementation() noexcept { return impl_.second(); }

    template <class Initiation>
    void emplace_stop_callback(StopToken&& stop_token, const Initiation& initiation) noexcept
    {
        impl_.first().emplace_stop_callback(static_cast<StopToken&&>(stop_token), initiation, implementation());
    }

    void reset_stop_callback() noexcept { impl_.first().reset_stop_callback(); }

  public:
    template <class R>
    BasicSenderRunningOperation(detail::AllocationType allocation_type, R&& receiver, Implementation&& implementation)
        : Base(get_on_complete(allocation_type)),
          impl_(static_cast<R&&>(receiver), static_cast<Implementation&&>(implementation))
    {
    }

    template <class R>
    BasicSenderRunningOperation(detail::AllocationType allocation_type, R&& receiver,
                                const Implementation& implementation)
        : Base(get_on_complete(allocation_type)), impl_(static_cast<R&&>(receiver), implementation)
    {
    }

    template <class Initiation>
    void start(agrpc::GrpcContext& grpc_context, const Initiation& initiation, StopToken&& stop_token) noexcept
    {
        grpc_context.work_started();
        emplace_stop_callback(static_cast<StopToken&&>(stop_token), initiation);
        initiate(grpc_context, initiation);
    }

    Receiver& receiver() noexcept { return impl_.first().receiver(); }

    void put_into_scratch_space(void* ptr) noexcept
    {
        detail::OperationBaseAccess::get_on_complete(*this) = reinterpret_cast<detail::OperationOnComplete>(ptr);
    }

    [[nodiscard]] void* get_scratch_space() const noexcept
    {
        return reinterpret_cast<void*>(detail::OperationBaseAccess::get_on_complete(*this));
    }

    void restore_scratch_space(detail::AllocationType allocation_type) noexcept
    {
        detail::OperationBaseAccess::get_on_complete(*this) = get_on_complete(allocation_type);
    }

  private:
    detail::CompressedPair<detail::ReceiverAndStopCallback<Receiver, StopFunction>, Implementation> impl_;
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
        op.restore_scratch_space(detail::AllocationType::NONE);
        auto stop_token = detail::check_start_conditions(grpc_context, receiver());
        if (stop_token)
        {
            op.start(grpc_context, impl_.second(), std::move(*stop_token));
        }
    }

  private:
    friend detail::BasicSender<Initiation, Implementation>;

    template <class R, class Impl>
    BasicSenderOperationState(R&& receiver, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                              Impl&& implementation)
        : impl_(detail::SecondThenVariadic{}, initiation, detail::AllocationType::NONE, static_cast<R&&>(receiver),
                static_cast<Impl&&>(implementation))
    {
        operation().put_into_scratch_space(&grpc_context);
    }

    auto& operation() noexcept { return impl_.first(); }

    Receiver& receiver() noexcept { return operation().receiver(); }

    detail::CompressedPair<
        detail::BasicSenderRunningOperation<Implementation, Receiver, detail::DeallocateOnComplete::NO>, Initiation>
        impl_;
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

#if !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT) && !defined(ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)
template <class Initiation, class Implementation, class R>
struct agrpc::asio::traits::submit_member<agrpc::detail::BasicSender<Initiation, Implementation>, R>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
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
