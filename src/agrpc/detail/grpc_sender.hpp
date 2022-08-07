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

#ifndef AGRPC_DETAIL_GRPC_SENDER_HPP
#define AGRPC_DETAIL_GRPC_SENDER_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/sender_allocation_traits.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Implementation>
class BasicGrpcSender;

struct BasicGrpcSenderAccess
{
    template <class Implementation>
    static auto create(agrpc::GrpcContext& grpc_context, Implementation&& implementation)
    {
        return detail::BasicGrpcSender<detail::RemoveCrefT<Implementation>>(
            grpc_context, std::forward<Implementation>(implementation));
    }
};

struct BasicGrpcSenderStarter
{
    template <class Operation>
    void operator()(agrpc::GrpcContext& grpc_context, Operation* operation)
    {
        operation->start(grpc_context);
    }
};

template <class Implementation>
class BasicGrpcSender : public detail::SenderOf<detail::GetSignatureT<Implementation, void(bool)>>
{
  private:
    using StopFunction = detail::GetStopFunctionT<Implementation, detail::Empty>;

    template <class Receiver, AllocationType AllocType>
    class RunningOperation;

    template <class Receiver>
    class OperationState
    {
      public:
        template <class R>
        OperationState(R&& receiver, agrpc::GrpcContext& grpc_context, Implementation&& implementation)
            : grpc_context(grpc_context), running(std::forward<R>(receiver), std::move(implementation))
        {
        }

        void start() noexcept { running.start(grpc_context); }

        agrpc::GrpcContext& grpc_context;
        RunningOperation<Receiver, AllocationType::NONE> running;
    };

  public:
    using Signature = detail::GetSignatureT<Implementation, void(bool)>;

    template <class Receiver>
    OperationState<detail::RemoveCrefT<Receiver>> connect(Receiver&& receiver) && noexcept(
        detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_copy_constructible_v<Implementation>)
    {
        return {std::forward<Receiver>(receiver), grpc_context, std::move(implementation)};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) &&
    {
        detail::BasicGrpcSenderStarter starter;
        detail::allocate_operation_and_invoke<detail::SenderOperationAllocationTraits<RunningOperation>>(
            grpc_context, starter, std::forward<Receiver>(receiver), std::move(implementation));
    }

  private:
    friend detail::BasicGrpcSenderAccess;

    explicit BasicGrpcSender(agrpc::GrpcContext& grpc_context, Implementation&& implementation) noexcept
        : grpc_context(grpc_context), implementation{std::move(implementation)}
    {
    }

    agrpc::GrpcContext& grpc_context;
    Implementation implementation;
};

template <class Implementation>
template <class Receiver, AllocationType AllocType>
class BasicGrpcSender<Implementation>::RunningOperation
    : public detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>
{
  public:
    template <class R>
    RunningOperation(R&& receiver, Implementation&& implementation)
        : detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>(
              &RunningOperation::operation_on_complete),
          receiver_and_stop_callback(std::forward<R>(receiver)),
          implementation{std::move(implementation)}
    {
    }

    void start(agrpc::GrpcContext& grpc_context) noexcept
    {
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
        {
            detail::exec::set_done(this->extract_receiver_and_optionally_deallocate(grpc_context.get_allocator()));
            return;
        }
        auto stop_token = detail::exec::get_stop_token(receiver());
        if (stop_token.stop_requested())
        {
            detail::exec::set_done(this->extract_receiver_and_optionally_deallocate(grpc_context.get_allocator()));
            return;
        }
        receiver_and_stop_callback.emplace_stop_callback(std::move(stop_token), implementation);
        detail::StartWorkAndGuard guard{grpc_context};
        implementation.initiate(grpc_context, this);
        guard.release();
    }

  private:
    struct Done
    {
        template <class... Args>
        void operator()(Args&&... args)
        {
            self_.receiver_and_stop_callback.reset_stop_callback();
            auto receiver = self_.extract_receiver_and_optionally_deallocate(local_allocator);
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                detail::satisfy_receiver(std::move(receiver), std::forward<Args>(args)...);
            }
            else
            {
                detail::exec::set_done(std::move(receiver));
            }
        }

        [[nodiscard]] RunningOperation* self() const noexcept { return &self_; }

        RunningOperation& self_;
        detail::InvokeHandler invoke_handler;
        detail::GrpcContextLocalAllocator local_allocator;
    };

    static void operation_on_complete(detail::TypeErasedGrpcTagOperation* op, detail::InvokeHandler invoke_handler,
                                      bool ok, detail::GrpcContextLocalAllocator local_allocator) noexcept
    {
        auto& self = *static_cast<RunningOperation*>(op);
        self.implementation.done(Done{self, invoke_handler, local_allocator}, ok);
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

    Receiver& receiver() noexcept { return receiver_and_stop_callback.receiver(); }

    detail::ReceiverAndStopCallback<Receiver, StopFunction> receiver_and_stop_callback;
    Implementation implementation;
};

template <class InitiatingFunction, class StopFunctionT>
struct SingleRpcStepSenderImplementation
{
    using StopFunction = StopFunctionT;

    auto create_stop_function() noexcept { return StopFunction{initiating_function}; }

    void initiate(agrpc::GrpcContext& grpc_context, void* self) { initiating_function(grpc_context, self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    InitiatingFunction initiating_function;
};

template <class InitiatingFunction, class StopFunction = detail::Empty>
using GrpcSender = detail::BasicGrpcSender<detail::SingleRpcStepSenderImplementation<InitiatingFunction, StopFunction>>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SENDER_HPP
