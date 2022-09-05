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

#ifndef AGRPC_DETAIL_REPEATEDLY_REQUEST_CONTEXT_HPP
#define AGRPC_DETAIL_REPEATEDLY_REQUEST_CONTEXT_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/repeatedly_request_base.hpp>
#include <agrpc/detail/rpc_context.hpp>
#include <agrpc/repeatedly_request_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RepeatedlyRequestContextAccess
{
    template <class ImplementationAllocator>
    static auto create(detail::AllocatedPointer<ImplementationAllocator>&& allocated_pointer) noexcept
    {
        return agrpc::RepeatedlyRequestContext{std::move(allocated_pointer)};
    }
};

template <class RequestHandler, class RPC, class CompletionHandler>
class RepeatedlyRequestOperation : public detail::TypeErasedGrpcTagOperation,
                                   public detail::TypeErasedNoArgOperation,
                                   public detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, CompletionHandler>
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;
    using NoArgBase = detail::TypeErasedNoArgOperation;
    using Service = detail::GetServiceT<RPC>;
    using RPCContext = detail::RPCContextForRPCT<RPC>;

  public:
    template <class Ch, class Rh>
    RepeatedlyRequestOperation(Rh&& request_handler, RPC rpc, Service& service, Ch&& completion_handler,
                               bool is_stoppable)
        : GrpcBase(&RepeatedlyRequestOperation::on_request_complete),
          NoArgBase(&detail::default_do_complete<RepeatedlyRequestOperation, detail::TypeErasedNoArgOperation>),
          detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, CompletionHandler>(
              static_cast<Rh&&>(request_handler), rpc, service, static_cast<Ch&&>(completion_handler), is_stoppable)
    {
    }

    bool initiate_repeatedly_request()
    {
        auto& local_grpc_context = this->grpc_context();
        if AGRPC_UNLIKELY (this->is_stopped())
        {
            return false;
        }
        auto next_rpc_context = detail::allocate<RPCContext>(this->get_allocator());
        this->rpc_context = next_rpc_context.get();
        auto* cq = local_grpc_context.get_server_completion_queue();
        local_grpc_context.work_started();
        detail::initiate_request_from_rpc_context(this->rpc(), this->service(), *rpc_context, cq, this);
        next_rpc_context.release();
        return true;
    }

  private:
    static void on_request_complete(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok,
                                    detail::GrpcContextLocalAllocator)
    {
        auto* self = static_cast<RepeatedlyRequestOperation*>(op);
        const auto allocator = self->get_allocator();
        detail::AllocatedPointer ptr{self->rpc_context, allocator};
        auto& grpc_context = self->grpc_context();
        auto& request_handler = self->request_handler();
        if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
        {
            if AGRPC_LIKELY (ok)
            {
                const auto is_repeated = self->initiate_repeatedly_request();
                detail::ScopeGuard guard{[&]
                                         {
                                             if AGRPC_UNLIKELY (!is_repeated)
                                             {
                                                 detail::GrpcContextImplementation::add_local_operation(grpc_context,
                                                                                                        self);
                                             }
                                         }};
                request_handler(detail::RepeatedlyRequestContextAccess::create(std::move(ptr)));
            }
            else
            {
                ptr.reset();
                detail::GrpcContextImplementation::add_local_operation(grpc_context, self);
            }
        }
        else
        {
            ptr.reset();
            detail::WorkFinishedOnExit on_exit{grpc_context};
            detail::destroy_deallocate(self, allocator);
        }
    }

    RPCContext* rpc_context;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLY_REQUEST_CONTEXT_HPP
