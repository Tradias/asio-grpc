// Copyright 2021 Dennis Hezel
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

#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc
{
struct ScheduleSender
{
  private:
    template <class Receiver>
    class Operation : private detail::TypeErasedNoArgOperation
    {
      public:
        template <class Receiver2>
        constexpr explicit Operation(const ScheduleSender& sender, Receiver2&& receiver)
            : detail::TypeErasedNoArgOperation(&Operation::on_complete),
              grpc_context(sender.grpc_context),
              receiver(std::forward<Receiver2>(receiver))
        {
        }

        void start() & noexcept
        {
            if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
            {
                detail::GrpcContextImplementation::add_local_operation(grpc_context, this);
            }
            else
            {
                detail::GrpcContextImplementation::add_remote_operation(grpc_context, this);
            }
        }

      private:
        static void on_complete(detail::TypeErasedNoArgOperation* op, detail::InvokeHandler,
                                detail::GrpcContextLocalAllocator) noexcept
        {
            auto& self = *static_cast<Operation*>(op);
            if constexpr (noexcept(detail::set_value(std::move(self.receiver))))
            {
                detail::set_value(std::move(self.receiver));
            }
            else
            {
                AGRPC_TRY { detail::set_value(std::move(self.receiver)); }
                AGRPC_CATCH(...) { detail::set_error(std::move(self.receiver), std::current_exception()); }
            }
        }

        agrpc::GrpcContext& grpc_context;
        Receiver receiver;
    };

  public:
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = Variant<Tuple<>>;

    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    static constexpr bool sends_done = false;

    constexpr explicit ScheduleSender(agrpc::GrpcContext& grpc_context) noexcept : grpc_context(grpc_context) {}

    template <class Receiver>
    constexpr Operation<detail::RemoveCvrefT<Receiver>> connect(Receiver&& receiver)
    {
        return Operation<detail::RemoveCvrefT<Receiver>>{*this, std::forward<Receiver>(receiver)};
    }

  private:
    agrpc::GrpcContext& grpc_context;
};
}  // namespace agrpc

#endif  // AGRPC_AGRPC_SCHEDULESENDER_HPP
