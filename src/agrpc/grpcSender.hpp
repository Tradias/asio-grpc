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
#include "agrpc/rpcs.hpp"

#ifdef AGRPC_UNIFEX
namespace agrpc
{
template <class InitiationFunction>
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
              initiation_function(sender.initiation_function),
              receiver(std::forward<Receiver2>(receiver))
        {
        }

        void start() & noexcept { initiation_function(context, this); }

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
        InitiationFunction initiation_function;
        Receiver receiver;
    };

  public:
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = Variant<Tuple<bool>>;

    // Note: Only case it might complete with exception_ptr is if the
    // receiver's set_value() exits with an exception.
    template <template <class...> class Variant>
    using error_types = Variant<std::error_code, std::exception_ptr>;

    static constexpr bool sends_done = true;

    explicit GrpcSender(agrpc::GrpcContext& context, InitiationFunction initiation_function) noexcept
        : context(context), initiation_function(std::move(initiation_function))
    {
    }

    template <class Receiver>
    Operation<detail::RemoveCvrefT<Receiver>> connect(Receiver&& receiver) &&
    {
        return Operation<detail::RemoveCvrefT<Receiver>>{*this, std::forward<Receiver>(receiver)};
    }

  private:
    agrpc::GrpcContext& context;
    InitiationFunction initiation_function;
};

template <class Scheduler, class RPC, class Service, class Request, class Responder>
auto tag_invoke(unifex::tag_t<agrpc::async_request>, Scheduler scheduler,
                detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                grpc::ServerContext& server_context, Request& request, Responder& responder) noexcept
{
    return agrpc::GrpcSender(scheduler.context(),
                             [&, rpc](agrpc::GrpcContext& grpc_context, void* tag)
                             {
                                 auto* cq = grpc_context.get_server_completion_queue();
                                 (service.*rpc)(&server_context, &request, &responder, cq, cq, tag);
                             });
}

template <class Scheduler, class Response>
auto tag_invoke(unifex::tag_t<agrpc::async_finish>, Scheduler scheduler,
                grpc::ServerAsyncResponseWriter<Response>& writer, const Response& response,
                const grpc::Status& status) noexcept
{
    return agrpc::GrpcSender(scheduler.context(),
                             [&](const agrpc::GrpcContext&, void* tag)
                             {
                                 writer.Finish(response, status, tag);
                             });
}

template <class Scheduler, class Response>
auto tag_invoke(unifex::tag_t<agrpc::async_finish>, Scheduler scheduler,
                grpc::ClientAsyncResponseReader<Response>& reader, Response& response, grpc::Status& status) noexcept
{
    return agrpc::GrpcSender(scheduler.context(),
                             [&](const agrpc::GrpcContext&, void* tag)
                             {
                                 reader.Finish(&response, &status, tag);
                             });
}
}  // namespace agrpc
#endif

#endif  // AGRPC_AGRPC_GRPCSENDER_HPP
