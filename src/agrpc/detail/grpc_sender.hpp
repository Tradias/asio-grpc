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
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/sender_operation.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class StopFunctionT, class ReceiverT, AllocationType AllocType>
struct RunningOperationTemplate
{
    static constexpr AllocationType ALLOCATION_TYPE = AllocType;

    using Receiver = ReceiverT;
    using StopFunction = StopFunctionT;

    struct Impl : detail::SenderOperation<RunningOperationTemplate, void(bool)>
    {
        using detail::SenderOperation<RunningOperationTemplate, void(bool)>::SenderOperation;

        template <class OnDone>
        void on_complete(OnDone on_done, bool ok)
        {
            on_done(ok);
        }
    };
};

template <class InitiatingFunction, class Operation>
bool grpc_sender_start(agrpc::GrpcContext& grpc_context, const InitiatingFunction& initiating_function,
                       Operation& operation) noexcept
{
    if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
    {
        detail::exec::set_done(std::move(operation.receiver()));
        return false;
    }
    auto stop_token = detail::exec::get_stop_token(operation.receiver());
    if (stop_token.stop_requested())
    {
        detail::exec::set_done(std::move(operation.receiver()));
        return false;
    }
    if constexpr (detail::GRPC_SENDER_HAS_STOP_CALLBACK<typename Operation::Receiver, typename Operation::StopFunction>)
    {
        operation.stop_callback().emplace(std::move(stop_token), typename Operation::StopFunction{initiating_function});
    }
    detail::StartWorkAndGuard guard{grpc_context};
    initiating_function(grpc_context, &operation);
    guard.release();
    return true;
}

template <class InitiatingFunction, class StopFunction = detail::Empty>
class GrpcSender : public detail::SenderOf<bool>
{
  private:
    template <class Receiver, AllocationType AllocType>
    using RunningOperation = typename detail::RunningOperationTemplate<StopFunction, Receiver, AllocType>::Impl;

    template <class Receiver>
    class Operation
    {
      public:
        void start() noexcept { detail::grpc_sender_start(grpc_context, initiating_function, running); }

      private:
        friend GrpcSender;

        template <class R>
        Operation(const GrpcSender& sender, R&& receiver)
            : grpc_context(sender.grpc_context),
              initiating_function(sender.initiating_function),
              running(std::forward<R>(receiver))
        {
        }

        agrpc::GrpcContext& grpc_context;
        InitiatingFunction initiating_function;
        RunningOperation<Receiver, AllocationType::NONE> running;
    };

    struct Starter
    {
        const InitiatingFunction& initiating_function;

        template <class Operation>
        void operator()(agrpc::GrpcContext& grpc_context, Operation* operation)
        {
            if (!detail::grpc_sender_start(grpc_context, initiating_function, *operation))
            {
                detail::extract_receiver_and_deallocate<Operation::ALLOCATION_TYPE>(*operation,
                                                                                    grpc_context.get_allocator());
            }
        }
    };

  public:
    template <class Receiver>
    auto connect(Receiver&& receiver) const noexcept(detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>)
        -> Operation<detail::RemoveCrefT<Receiver>>
    {
        return {*this, std::forward<Receiver>(receiver)};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) const
    {
        Starter starter{initiating_function};
        detail::allocate_operation_and_invoke<detail::SenderOperationAllocationTraits<RunningOperation>>(
            grpc_context, std::forward<Receiver>(receiver), starter);
    }

  private:
    explicit GrpcSender(agrpc::GrpcContext& grpc_context, InitiatingFunction initiating_function) noexcept
        : grpc_context(grpc_context), initiating_function(std::move(initiating_function))
    {
    }

    friend detail::GrpcInitiateImplFn;

    agrpc::GrpcContext& grpc_context;
    InitiatingFunction initiating_function;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SENDER_HPP
