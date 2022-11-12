// Copyright 2022 Dennis Hezel
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
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
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
[[nodiscard]] std::optional<detail::exec::stop_token_type_t<Receiver>> check_start_conditions(
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

template <class Implementation>
class BasicSender;

struct BasicSenderAccess
{
    template <class Implementation>
    static auto create(agrpc::GrpcContext& grpc_context, const typename Implementation::Initiation& initiation,
                       Implementation&& implementation)
    {
        return detail::BasicSender<detail::RemoveCrefT<Implementation>>(grpc_context, initiation,
                                                                        static_cast<Implementation&&>(implementation));
    }
};

enum class DeallocateOnComplete
{
    NO,
    YES
};

template <class Implementation, class Receiver, detail::DeallocateOnComplete Deallocate>
class BasicSenderRunningOperation;

template <class Implementation>
struct BasicSenderRunningOperationTemplate
{
    template <class Receiver>
    using Type = detail::BasicSenderRunningOperation<Implementation, Receiver, detail::DeallocateOnComplete::YES>;
};

template <class Receiver, class Implementation>
void submit_basic_sender_running_operation(agrpc::GrpcContext& grpc_context, Receiver receiver,
                                           const typename detail::RemoveCrefT<Implementation>::Initiation& initiation,
                                           Implementation&& implementation)
{
    if (auto stop_token = detail::check_start_conditions(grpc_context, receiver))
    {
        auto operation = detail::allocate_operation<
            detail::BasicSenderRunningOperationTemplate<detail::RemoveCrefT<Implementation>>::template Type>(
            grpc_context, static_cast<Receiver&&>(receiver), static_cast<Implementation&&>(implementation));
        operation->start(grpc_context, initiation, std::move(*stop_token));
    }
}

template <class Implementation, class Receiver>
class BasicSenderOperationState;

template <class Implementation>
class BasicSender : public detail::SenderOf<typename Implementation::Signature>
{
  private:
    using Initiation = typename Implementation::Initiation;

  public:
    template <class Receiver>
    detail::BasicSenderOperationState<Implementation, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) && noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                          std::is_nothrow_copy_constructible_v<Initiation> &&
                                          std::is_nothrow_move_constructible_v<Implementation>))
    {
        return {static_cast<Receiver&&>(receiver), grpc_context, initiation,
                static_cast<Implementation&&>(implementation)};
    }

    template <class Receiver, class Impl = Implementation, class = std::enable_if_t<std::is_copy_constructible_v<Impl>>>
    detail::BasicSenderOperationState<Implementation, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) const& noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                              std::is_nothrow_copy_constructible_v<Initiation> &&
                                              std::is_nothrow_copy_constructible_v<Implementation>))
    {
        return {static_cast<Receiver&&>(receiver), grpc_context, initiation, implementation};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) &&
    {
        detail::submit_basic_sender_running_operation(grpc_context, static_cast<Receiver&&>(receiver), initiation,
                                                      static_cast<Implementation&&>(implementation));
    }

    template <class Receiver, class Impl = Implementation, class = std::enable_if_t<std::is_copy_constructible_v<Impl>>>
    void submit(Receiver&& receiver) const&
    {
        detail::submit_basic_sender_running_operation(grpc_context, static_cast<Receiver&&>(receiver), initiation,
                                                      implementation);
    }

  private:
    friend detail::BasicSenderAccess;

    template <class, class>
    friend class BasicSenderOperationState;

    BasicSender(agrpc::GrpcContext& grpc_context, const Initiation& initiation, Implementation&& implementation)
        : grpc_context(grpc_context),
          initiation(initiation),
          implementation{static_cast<Implementation&&>(implementation)}
    {
    }

    agrpc::GrpcContext& grpc_context;
    Initiation initiation;
    Implementation implementation;
};

template <class Implementation, class Receiver, detail::DeallocateOnComplete Deallocate>
class BasicSenderRunningOperation : public detail::BasicSenderRunningOperationBase<Implementation::TYPE>
{
  private:
    using Base = detail::BasicSenderRunningOperationBase<Implementation::TYPE>;
    using Initiation = typename Implementation::Initiation;
    using StopFunction = typename Implementation::StopFunction;
    using StopToken = detail::exec::stop_token_type_t<Receiver>;

    template <detail::AllocationType AllocType>
    struct OnDone
    {
        template <int Id>
        struct Type
        {
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
                    self_.set_on_complete(BasicSenderRunningOperation::get_on_complete_impl<AllocType, NextId>());
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

    template <detail::AllocationType AllocType, int Id, class TypeErasedOperation, class... Args>
    static void on_complete_impl(TypeErasedOperation* op, detail::InvokeHandler invoke_handler, Args... args,
                                 agrpc::GrpcContext& grpc_context)
    {
        auto& self = *static_cast<BasicSenderRunningOperation*>(op);
        if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
        {
            self.implementation().done(typename OnDone<AllocType>::template Type<Id>{self, grpc_context},
                                       static_cast<Args&&>(args)...);
        }
        else
        {
            detail::exec::set_done(self.extract_receiver_and_optionally_deallocate<AllocType>(grpc_context));
        }
    }

    template <detail::AllocationType AllocType, int Id = 0>
    static detail::BasicSenderRunningOperationBaseArg get_on_complete_impl()
    {
        if constexpr (Implementation::TYPE == detail::SenderImplementationType::BOTH)
        {
            return {&on_complete_impl<AllocType, Id, detail::TypeErasedNoArgOperation>,
                    &on_complete_impl<AllocType, Id, detail::TypeErasedGrpcTagOperation, bool>};
        }
        else if constexpr (Implementation::TYPE == detail::SenderImplementationType::NO_ARG)
        {
            return {&on_complete_impl<AllocType, Id, detail::TypeErasedNoArgOperation>, nullptr};
        }
        else
        {
            return {nullptr, &on_complete_impl<AllocType, Id, detail::TypeErasedGrpcTagOperation, bool>};
        }
    }

    static detail::BasicSenderRunningOperationBaseArg get_on_complete(
        [[maybe_unused]] detail::AllocationType allocation_type)
    {
        if constexpr (Deallocate == detail::DeallocateOnComplete::YES)
        {
            if (allocation_type == detail::AllocationType::LOCAL)
            {
                return get_on_complete_impl<detail::AllocationType::LOCAL>();
            }
            else
            {
                return get_on_complete_impl<detail::AllocationType::CUSTOM>();
            }
        }
        else
        {
            return get_on_complete_impl<detail::AllocationType::NONE>();
        }
    }

    template <detail::AllocationType AllocType>
    struct Init
    {
        template <int NextId = 0>
        [[nodiscard]] Base* self() const noexcept
        {
            if constexpr (NextId != 0)
            {
                self_.set_on_complete(BasicSenderRunningOperation::get_on_complete_impl<AllocType, NextId>());
            }
            return &self_;
        }

        [[nodiscard]] agrpc::GrpcContext& grpc_context() const noexcept { return grpc_context_; }

        BasicSenderRunningOperation& self_;
        agrpc::GrpcContext& grpc_context_;
    };

    template <class... T>
    void initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation, T...)
    {
        if constexpr (Deallocate == detail::DeallocateOnComplete::YES)
        {
            if (this->on_complete_equals(get_on_complete_impl<detail::AllocationType::LOCAL>()))
            {
                this->implementation().initiate(Init<detail::AllocationType::LOCAL>{*this, grpc_context}, initiation);
            }
            else
            {
                this->implementation().initiate(Init<detail::AllocationType::CUSTOM>{*this, grpc_context}, initiation);
            }
        }
        else
        {
            this->implementation().initiate(Init<detail::AllocationType::NONE>{*this, grpc_context}, initiation);
        }
    }

    template <class = decltype((void)std::declval<Implementation&>().initiate(
                  std::declval<agrpc::GrpcContext&>(), std::declval<const Initiation&>(), static_cast<Base*>(nullptr)))>
    void initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation)
    {
        this->implementation().initiate(grpc_context, initiation, static_cast<Base*>(this));
    }

    Implementation& implementation() noexcept { return impl.second(); }

    void emplace_stop_callback(StopToken&& stop_token, const Initiation& initiation) noexcept
    {
        if (stop_token.stop_possible())
        {
            impl.first().emplace_stop_callback(static_cast<StopToken&&>(stop_token), this->implementation(),
                                               initiation);
        }
    }

    void reset_stop_callback() noexcept { impl.first().reset_stop_callback(); }

  public:
    template <class R>
    BasicSenderRunningOperation(detail::AllocationType allocation_type, R&& receiver, Implementation&& implementation)
        : Base(get_on_complete(allocation_type)),
          impl(static_cast<R&&>(receiver), static_cast<Implementation&&>(implementation))
    {
    }

    template <class R>
    BasicSenderRunningOperation(detail::AllocationType allocation_type, R&& receiver,
                                const Implementation& implementation)
        : Base(get_on_complete(allocation_type)), impl(static_cast<R&&>(receiver), implementation)
    {
    }

    void start(agrpc::GrpcContext& grpc_context, const Initiation& initiation, StopToken&& stop_token) noexcept
    {
        grpc_context.work_started();
        this->emplace_stop_callback(static_cast<StopToken&&>(stop_token), initiation);
        this->initiate(grpc_context, initiation);
    }

    Receiver& receiver() noexcept { return impl.first().receiver(); }

  private:
    detail::CompressedPair<detail::ReceiverAndStopCallback<Receiver, StopFunction>, Implementation> impl;
};

template <class Implementation, class Receiver>
class BasicSenderOperationState
{
  public:
    void start() noexcept
    {
        auto stop_token = detail::check_start_conditions(grpc_context, receiver());
        if (stop_token)
        {
            impl.first().start(grpc_context, impl.second(), std::move(*stop_token));
        }
    }

  private:
    using Initiation = typename Implementation::Initiation;

    friend detail::BasicSender<Implementation>;

    template <class, class, class...>
    friend class ConditionalSenderOperationState;

    template <class R>
    BasicSenderOperationState(R&& receiver, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                              Implementation&& implementation)
        : grpc_context(grpc_context),
          impl(detail::SecondThenVariadic{}, initiation, detail::AllocationType::NONE, static_cast<R&&>(receiver),
               static_cast<Implementation&&>(implementation))
    {
    }

    template <class R>
    BasicSenderOperationState(R&& receiver, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                              const Implementation& implementation)
        : grpc_context(grpc_context),
          impl(detail::SecondThenVariadic{}, initiation, detail::AllocationType::NONE, static_cast<R&&>(receiver),
               implementation)
    {
    }

    Receiver& receiver() noexcept { return impl.first().receiver(); }

    agrpc::GrpcContext& grpc_context;
    detail::CompressedPair<
        detail::BasicSenderRunningOperation<Implementation, Receiver, detail::DeallocateOnComplete::NO>, Initiation>
        impl;
};
}

AGRPC_NAMESPACE_END

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)
template <class Implementation, class R>
struct agrpc::asio::traits::connect_member<agrpc::detail::BasicSender<Implementation>, R>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept =
        noexcept(std::declval<agrpc::detail::BasicSender<Implementation>>().connect(std::declval<R>()));

    using result_type = decltype(std::declval<agrpc::detail::BasicSender<Implementation>>().connect(std::declval<R>()));
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_SUBMIT_MEMBER_TRAIT)
template <class Implementation, class R>
struct agrpc::asio::traits::submit_member<agrpc::detail::BasicSender<Implementation>, R>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)
template <class Implementation, class Receiver>
struct agrpc::asio::traits::start_member<agrpc::detail::BasicSenderOperationState<Implementation, Receiver>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};
#endif

#endif  // AGRPC_DETAIL_BASIC_SENDER_HPP
