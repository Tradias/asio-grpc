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

#ifndef AGRPC_AGRPC_SCHEDULESENDER_HPP
#define AGRPC_AGRPC_SCHEDULESENDER_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/receiver.hpp"
#include "agrpc/detail/senderOf.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class ScheduleSender : public detail::SenderOf<>
{
  private:
    template <class Receiver>
    class Operation : private detail::TypeErasedNoArgOperation
    {
      public:
        template <class Receiver2>
        constexpr Operation(const ScheduleSender& sender, Receiver2&& receiver)
            : detail::TypeErasedNoArgOperation(&Operation::on_complete),
              impl(sender.grpc_context, std::forward<Receiver2>(receiver))
        {
        }

        void start() & noexcept
        {
            if AGRPC_UNLIKELY (this->grpc_context().is_stopped())
            {
                detail::set_done(std::move(this->receiver()));
                return;
            }
            this->grpc_context().work_started();
            detail::GrpcContextImplementation::add_operation(this->grpc_context(), this);
        }

      private:
        static void on_complete(detail::TypeErasedNoArgOperation* op, detail::InvokeHandler invoke_handler,
                                detail::GrpcContextLocalAllocator) noexcept
        {
            auto& self = *static_cast<Operation*>(op);
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                detail::satisfy_receiver(std::move(self.receiver()));
            }
            else
            {
                detail::set_done(std::move(self.receiver()));
            }
        }

        constexpr decltype(auto) grpc_context() noexcept { return impl.first(); }

        constexpr decltype(auto) receiver() noexcept { return impl.second(); }

        detail::CompressedPair<agrpc::GrpcContext&, Receiver> impl;
    };

  public:
    template <class Receiver>
    constexpr auto connect(Receiver&& receiver) const noexcept(std::is_nothrow_constructible_v<Receiver, Receiver&&>)
        -> Operation<detail::RemoveCvrefT<Receiver>>
    {
        return {*this, std::forward<Receiver>(receiver)};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) const
    {
        auto allocator = detail::get_allocator(receiver);
        if (!detail::create_and_submit_no_arg_operation_if_not_stopped<true>(
                this->grpc_context,
                [receiver = detail::RemoveCvrefT<Receiver>{std::forward<Receiver>(receiver)}]() mutable
                {
                    detail::satisfy_receiver(std::move(receiver));
                },
                allocator))
        {
            detail::set_done(std::forward<Receiver>(receiver));
        }
    }

  private:
    constexpr explicit ScheduleSender(agrpc::GrpcContext& grpc_context) noexcept : grpc_context(grpc_context) {}

    template <class Allocator, std::uint32_t Options>
    friend class agrpc::BasicGrpcExecutor;

    agrpc::GrpcContext& grpc_context;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SCHEDULESENDER_HPP
