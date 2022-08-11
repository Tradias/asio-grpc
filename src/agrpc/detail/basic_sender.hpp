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
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Implementation>
class BasicSender;

struct BasicSenderAccess
{
    template <class Implementation>
    static auto create(agrpc::GrpcContext& grpc_context, const typename Implementation::Initiation& initiation,
                       Implementation&& implementation)
    {
        return detail::BasicSender<detail::RemoveCrefT<Implementation>>(grpc_context, initiation,
                                                                        std::forward<Implementation>(implementation));
    }
};

template <class Initiation>
struct BasicSenderStarter
{
    template <class Operation>
    void operator()(agrpc::GrpcContext& grpc_context, Operation* operation)
    {
        operation->start(grpc_context, initiation);
    }

    Initiation initiation;
};

enum class AllocationType
{
    NONE,
    LOCAL,
    CUSTOM
};

template <template <class, AllocationType> class Operation>
struct BasicSenderOperationAllocationTraits
{
    template <class Receiver>
    using Local = Operation<Receiver, AllocationType::LOCAL>;

    template <class Receiver>
    using Custom = Operation<Receiver, AllocationType::CUSTOM>;
};

template <class Implementation, class Receiver>
class BasicSenderOperationState;

template <class Implementation>
class BasicSender : public detail::SenderOf<typename Implementation::Signature>
{
  private:
    using StopFunction = typename Implementation::StopFunction;
    using Initiation = typename Implementation::Initiation;

    template <class Receiver, AllocationType AllocType>
    class RunningOperation;

  public:
    using Signature = typename Implementation::Signature;

    template <class Receiver>
    detail::BasicSenderOperationState<Implementation, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) && noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                          std::is_nothrow_copy_constructible_v<Initiation> &&
                                          std::is_nothrow_move_constructible_v<Implementation>))
    {
        return {std::forward<Receiver>(receiver), grpc_context, initiation, std::move(implementation)};
    }

    template <class Receiver, class = std::enable_if_t<std::is_copy_constructible_v<Implementation>>>
    detail::BasicSenderOperationState<Implementation, detail::RemoveCrefT<Receiver>> connect(Receiver&& receiver) const
        noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                  std::is_nothrow_copy_constructible_v<Initiation> &&
                  std::is_nothrow_copy_constructible_v<Implementation>))
    {
        return {std::forward<Receiver>(receiver), grpc_context, initiation, implementation};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) &&
    {
        detail::BasicSenderStarter<Initiation> starter{initiation};
        detail::allocate_operation_and_invoke<detail::BasicSenderOperationAllocationTraits<RunningOperation>>(
            grpc_context, starter, std::forward<Receiver>(receiver), std::move(implementation));
    }

    template <class Receiver, class = std::enable_if_t<std::is_copy_constructible_v<Implementation>>>
    void submit(Receiver&& receiver) const
    {
        detail::BasicSenderStarter<Initiation> starter{initiation};
        detail::allocate_operation_and_invoke<detail::BasicSenderOperationAllocationTraits<RunningOperation>>(
            grpc_context, starter, std::forward<Receiver>(receiver), implementation);
    }

  private:
    friend detail::BasicSenderAccess;

    template <class, class>
    friend class BasicSenderOperationState;

    BasicSender(agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                Implementation&& implementation) noexcept
        : grpc_context(grpc_context), initiation(initiation), implementation{std::move(implementation)}
    {
    }

    agrpc::GrpcContext& grpc_context;
    Initiation initiation;
    Implementation implementation;
};

struct EmptyGrpcTagTypeErasedBase
{
    template <class Arg1>
    constexpr explicit EmptyGrpcTagTypeErasedBase(Arg1&&) noexcept
    {
    }
};

struct EmptyNoArgTypeErasedBase
{
    template <class Arg1>
    constexpr explicit EmptyNoArgTypeErasedBase(Arg1&&) noexcept
    {
    }
};

template <class Implementation>
template <class Receiver, AllocationType AllocType>
class BasicSender<Implementation>::RunningOperation
    : public detail::GetGrpcTagTypeErasedBaseT<Implementation, detail::EmptyGrpcTagTypeErasedBase>,
      public detail::GetNoArgTypeErasedBaseT<Implementation, detail::EmptyNoArgTypeErasedBase>
{
  private:
    using GrpcTagBase = detail::GetGrpcTagTypeErasedBaseT<Implementation, detail::EmptyGrpcTagTypeErasedBase>;
    using NoArgBase = detail::GetNoArgTypeErasedBaseT<Implementation, detail::EmptyNoArgTypeErasedBase>;

    struct DoneDuringInitiate
    {
        DoneDuringInitiate(RunningOperation* self_, agrpc::GrpcContext& grpc_context, Initiation& initiation)
            : self_(self_), grpc_context(grpc_context), initiation_(initiation)
        {
            grpc_context.work_started();
        }

        DoneDuringInitiate(DoneDuringInitiate&& other)
            : self_(std::exchange(other.self_, nullptr)), grpc_context(grpc_context)
        {
        }

        DoneDuringInitiate& operator=(DoneDuringInitiate&&) = delete;

        ~DoneDuringInitiate()
        {
            if (!self_)
            {
                grpc_context.work_finished();
            }
        }

        template <class... Args>
        void operator()(Args... args)
        {
            self_->reset_stop_callback();
            auto receiver = self_->extract_receiver_and_optionally_deallocate(grpc_context.get_allocator());
            self_ = nullptr;
            detail::satisfy_receiver(std::move(receiver), static_cast<Args&&>(args)...);
        }

        [[nodiscard]] RunningOperation* self() const noexcept { return self_; }

        [[nodiscard]] Initiation& initiation() const noexcept { return initiation_; }

        [[nodiscard]] Initiation* operator->() const noexcept { return &initiation_; }

        [[nodiscard]] operator void*() const noexcept { return static_cast<void*>(self_); }

        RunningOperation* self_;
        agrpc::GrpcContext& grpc_context;
        Initiation& initiation_;
    };

    struct Done
    {
        template <class... Args>
        void operator()(Args... args)
        {
            self_.reset_stop_callback();
            auto receiver = self_.extract_receiver_and_optionally_deallocate(local_allocator);
            detail::satisfy_receiver(std::move(receiver), static_cast<Args&&>(args)...);
        }

        [[nodiscard]] RunningOperation* self() const noexcept { return &self_; }

        RunningOperation& self_;
        detail::GrpcContextLocalAllocator local_allocator;
    };

  public:
    template <class R>
    RunningOperation(R&& receiver, Implementation&& implementation)
        : GrpcTagBase(&RunningOperation::grpc_tag_on_complete),
          NoArgBase(&RunningOperation::no_arg_on_complete),
          impl(std::forward<R>(receiver), std::move(implementation))
    {
    }

    template <class R>
    RunningOperation(R&& receiver, const Implementation& implementation)
        : GrpcTagBase(&RunningOperation::grpc_tag_on_complete),
          NoArgBase(&RunningOperation::no_arg_on_complete),
          impl(std::forward<R>(receiver), implementation)
    {
    }

    RunningOperation(const RunningOperation&) = delete;
    RunningOperation(RunningOperation&&) = delete;
    RunningOperation& operator=(const RunningOperation&) = delete;
    RunningOperation& operator=(RunningOperation&&) = delete;

    void start(agrpc::GrpcContext& grpc_context, Initiation& initiation) noexcept
    {
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
        {
            detail::exec::set_done(this->extract_receiver_and_optionally_deallocate(grpc_context.get_allocator()));
            return;
        }
        auto stop_token = detail::exec::get_stop_token(receiver());
        if constexpr (!std::is_same_v<detail::exec::unstoppable_token, decltype(stop_token)>)
        {
            if (stop_token.stop_requested())
            {
                detail::exec::set_done(this->extract_receiver_and_optionally_deallocate(grpc_context.get_allocator()));
                return;
            }
            this->emplace_stop_callback(std::move(stop_token), initiation);
        }
        implementation().initiate(grpc_context, DoneDuringInitiate{this, grpc_context, initiation});
    }

  private:
    template <class... Args>
    static void on_complete_impl(RunningOperation& self, detail::InvokeHandler invoke_handler,
                                 detail::GrpcContextLocalAllocator local_allocator, Args&&... args) noexcept
    {
        if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
        {
            self.implementation().done(Done{self, local_allocator}, static_cast<Args&&>(args)...);
        }
        else
        {
            detail::exec::set_done(std::move(self.receiver()));
        }
    }

    static void grpc_tag_on_complete([[maybe_unused]] detail::TypeErasedGrpcTagOperation* op,
                                     [[maybe_unused]] detail::InvokeHandler invoke_handler, [[maybe_unused]] bool ok,
                                     [[maybe_unused]] detail::GrpcContextLocalAllocator local_allocator) noexcept
    {
        if constexpr (!std::is_same_v<EmptyGrpcTagTypeErasedBase, GrpcTagBase>)
        {
            on_complete_impl(*static_cast<RunningOperation*>(op), invoke_handler, local_allocator, ok);
        }
    }

    static void no_arg_on_complete([[maybe_unused]] detail::TypeErasedNoArgOperation* op,
                                   [[maybe_unused]] detail::InvokeHandler invoke_handler,
                                   [[maybe_unused]] detail::GrpcContextLocalAllocator local_allocator) noexcept
    {
        if constexpr (!std::is_same_v<EmptyNoArgTypeErasedBase, NoArgBase>)
        {
            on_complete_impl(*static_cast<RunningOperation*>(op), invoke_handler, local_allocator);
        }
    }

    auto extract_receiver_and_deallocate(detail::GrpcContextLocalAllocator local_allocator) noexcept
    {
        const auto& allocator = [&]
        {
            if constexpr (AllocType == AllocationType::LOCAL)
            {
                return local_allocator;
            }
            else
            {
                return detail::exec::get_allocator(receiver());
            }
        }();
        auto local_receiver{std::move(receiver())};
        detail::destroy_deallocate(this, allocator);
        return local_receiver;
    }

    auto extract_receiver_and_optionally_deallocate(detail::GrpcContextLocalAllocator local_allocator) noexcept
    {
        if constexpr (AllocType == AllocationType::NONE)
        {
            return std::move(receiver());
        }
        else
        {
            return this->extract_receiver_and_deallocate(local_allocator);
        }
    }

    Receiver& receiver() noexcept { return impl.first().receiver(); }

    Implementation& implementation() noexcept { return impl.second(); }

    template <class StopToken>
    void emplace_stop_callback(StopToken&& stop_token, Initiation& initiation) noexcept
    {
        impl.first().emplace_stop_callback(std::forward<StopToken>(stop_token), initiation);
    }

    void reset_stop_callback() noexcept { impl.first().reset_stop_callback(); }

    detail::CompressedPair<detail::ReceiverAndStopCallback<Receiver, StopFunction>, Implementation> impl;
};

template <class Implementation, class Receiver>
class BasicSenderOperationState
{
  public:
    void start() noexcept { impl.first().start(grpc_context, impl.second()); }

  private:
    using Initiation = typename Implementation::Initiation;

    friend detail::BasicSender<Implementation>;

    template <class R>
    BasicSenderOperationState(R&& receiver, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                              Implementation&& implementation)
        : grpc_context(grpc_context),
          impl(detail::SecondThenVariadic{}, initiation, std::forward<R>(receiver), std::move(implementation))
    {
    }

    template <class R>
    BasicSenderOperationState(R&& receiver, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                              const Implementation& implementation)
        : grpc_context(grpc_context),
          impl(detail::SecondThenVariadic{}, initiation, std::forward<R>(receiver), implementation)
    {
    }

    agrpc::GrpcContext& grpc_context;
    detail::CompressedPair<
        typename detail::BasicSender<Implementation>::template RunningOperation<Receiver, AllocationType::NONE>,
        Initiation>
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
