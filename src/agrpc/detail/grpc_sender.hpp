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
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Implementation, class StopFunction = detail::Empty>
class BasicGrpcSender;

struct BasicGrpcSenderAccess
{
    template <class Implementation>
    static auto create(Implementation&& implementation)
    {
        return detail::BasicGrpcSender<detail::RemoveCrefT<Implementation>>(
            std::forward<Implementation>(implementation));
    }
};

struct BasicGrpcSenderStarter
{
    template <class Operation>
    void operator()(agrpc::GrpcContext&, Operation* operation)
    {
        operation->start();
    }
};

template <class Implementation, class StopFunction>
class BasicGrpcSender : public detail::SenderOf<bool>
{
  private:
    template <class Receiver, AllocationType AllocType>
    class Operation;

  public:
    template <class Receiver>
    auto connect(Receiver&& receiver) const noexcept(
        detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_copy_constructible_v<Implementation>)
        -> Operation<detail::RemoveCrefT<Receiver>, AllocationType::NONE>
    {
        return {std::forward<Receiver>(receiver), implementation};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) const
    {
        detail::BasicGrpcSenderStarter starter;
        detail::allocate_operation_and_invoke<detail::SenderOperationAllocationTraits<Operation>>(
            implementation.grpc_context, starter, std::forward<Receiver>(receiver), implementation);
    }

  private:
    template <class... Args>
    explicit BasicGrpcSender(Args&&... args) noexcept : implementation{std::forward<Args>(args)...}
    {
    }

    friend detail::BasicGrpcSenderAccess;
    friend detail::GrpcInitiateImplFn;

    Implementation implementation;
};

template <class Implementation, class StopFunction>
template <class Receiver, AllocationType AllocType>
class BasicGrpcSender<Implementation, StopFunction>::Operation
    : public detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>
{
  public:
    template <class R>
    Operation(R&& receiver, const Implementation& implementation)
        : detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>(
              &Operation::operation_on_complete),
          receiver_and_stop_callback(std::forward<R>(receiver)),
          implementation{implementation}
    {
    }

    void start() noexcept
    {
        agrpc::GrpcContext& grpc_context = implementation.grpc_context;
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
        {
            detail::exec::set_done(this->extract_receiver_and_optionally_deallocate());
            return;
        }
        auto stop_token = detail::exec::get_stop_token(receiver());
        if (stop_token.stop_requested())
        {
            detail::exec::set_done(this->extract_receiver_and_optionally_deallocate());
            return;
        }
        receiver_and_stop_callback.emplace_stop_callback(std::move(stop_token), implementation);
        detail::StartWorkAndGuard guard{grpc_context};
        implementation.initiate(this);
        guard.release();
    }

  private:
    struct Done
    {
        template <class... Args>
        void operator()(Args&&... args)
        {
            self_.receiver_and_stop_callback.reset_stop_callback();
            auto receiver = self_.extract_receiver_and_optionally_deallocate();
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                detail::satisfy_receiver(std::move(receiver), std::forward<Args>(args)...);
            }
            else
            {
                detail::exec::set_done(std::move(receiver));
            }
        }

        [[nodiscard]] Operation* self() const noexcept { return &self_; }

        Operation& self_;
        detail::InvokeHandler invoke_handler;
    };

    static void operation_on_complete(detail::TypeErasedGrpcTagOperation* op, detail::InvokeHandler invoke_handler,
                                      bool ok, detail::GrpcContextLocalAllocator) noexcept
    {
        auto& self = *static_cast<Operation*>(op);
        self.implementation.done(Done{self, invoke_handler}, ok);
    }

    auto extract_receiver_and_deallocate() noexcept
    {
        const auto& allocator = [&]
        {
            if constexpr (AllocType == AllocationType::LOCAL)
            {
                return grpc_context().get_allocator();
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

    auto extract_receiver_and_optionally_deallocate() noexcept
    {
        if constexpr (AllocType != AllocationType::NONE)
        {
            return this->extract_receiver_and_deallocate();
        }
        else
        {
            return std::move(receiver());
        }
    }

    agrpc::GrpcContext& grpc_context() const noexcept { return implementation.grpc_context; }

    Receiver& receiver() noexcept { return receiver_and_stop_callback.receiver(); }

    detail::ReceiverAndStopCallback<Receiver, StopFunction> receiver_and_stop_callback;
    Implementation implementation;
};

template <class InitiatingFunction>
struct SingleRpcStepSenderImplementation
{
    SingleRpcStepSenderImplementation(agrpc::GrpcContext& grpc_context, const InitiatingFunction& initiating_function)
        : grpc_context(grpc_context), initiating_function(initiating_function)
    {
    }

    template <class StopFunction>
    auto create_stop_function() noexcept
    {
        return StopFunction{initiating_function};
    }

    void initiate(void* self) { initiating_function(grpc_context, self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    agrpc::GrpcContext& grpc_context;
    InitiatingFunction initiating_function;
};

template <class InitiatingFunction, class StopFunction = detail::Empty>
using GrpcSender = detail::BasicGrpcSender<detail::SingleRpcStepSenderImplementation<InitiatingFunction>, StopFunction>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SENDER_HPP
