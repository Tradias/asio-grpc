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

#ifndef AGRPC_AGRPC_GRPCSENDER_HPP
#define AGRPC_AGRPC_GRPCSENDER_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/grpcContext.hpp"
#include "agrpc/detail/typeErasedOperation.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"

#ifdef AGRPC_UNIFEX
namespace agrpc
{
template <class InitiatingFunction>
class GrpcSender
{
  private:
    template <class Receiver>
    class Operation : private detail::TypeErasedGrpcTagOperation
    {
      public:
        template <class Receiver2>
        explicit Operation(const GrpcSender& sender, Receiver2&& receiver)
            : detail::TypeErasedGrpcTagOperation(&Operation::on_complete),
              context(sender.context),
              initiating_function(sender.initiating_function),
              receiver(std::forward<Receiver2>(receiver))
        {
        }

        void start() & noexcept { initiating_function(context, this); }

      private:
        static void on_complete(detail::TypeErasedGrpcTagOperation* op, detail::InvokeHandler, bool ok,
                                detail::GrpcContextLocalAllocator) noexcept
        {
            auto& self = *static_cast<Operation*>(op);
            if constexpr (noexcept(unifex::set_value(std::move(self.receiver), ok)))
            {
                unifex::set_value(std::move(self.receiver), ok);
            }
            else
            {
                UNIFEX_TRY { unifex::set_value(std::move(self.receiver), ok); }
                UNIFEX_CATCH(...) { unifex::set_error(std::move(self.receiver), std::current_exception()); }
            }
        }

        agrpc::GrpcContext& context;
        InitiatingFunction initiating_function;
        Receiver receiver;
    };

  public:
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = Variant<Tuple<bool>>;

    // Note: Only case it might complete with exception_ptr is if the
    // receiver's set_value() exits with an exception.
    template <template <class...> class Variant>
    using error_types = Variant<std::error_code, std::exception_ptr>;

    static constexpr bool sends_done = false;

    explicit GrpcSender(agrpc::GrpcContext& context, InitiatingFunction initiating_function) noexcept
        : context(context), initiating_function(std::move(initiating_function))
    {
    }

    template <class Receiver>
    Operation<detail::RemoveCvrefT<Receiver>> connect(Receiver&& receiver) &&
    {
        return Operation<detail::RemoveCvrefT<Receiver>>{*this, std::forward<Receiver>(receiver)};
    }

  private:
    agrpc::GrpcContext& context;
    InitiatingFunction initiating_function;
};
}  // namespace agrpc
#endif

#endif  // AGRPC_AGRPC_GRPCSENDER_HPP
