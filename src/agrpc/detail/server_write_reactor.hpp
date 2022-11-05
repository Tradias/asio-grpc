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

#ifndef AGRPC_DETAIL_SERVER_WRITE_REACTOR_HPP
#define AGRPC_DETAIL_SERVER_WRITE_REACTOR_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/type_erased_operation.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct ServerWriteReactorStepBase : detail::TypeErasedGrpcTagOperation
{
    using detail::TypeErasedGrpcTagOperation::TypeErasedGrpcTagOperation;
};

struct ServerWriteReactorDoneBase : detail::TypeErasedGrpcTagOperation
{
    using detail::TypeErasedGrpcTagOperation::TypeErasedGrpcTagOperation;
};

template <class Derived, class Response>
class ServerWriteReactor : public detail::ServerWriteReactorStepBase, public detail::ServerWriteReactorDoneBase
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;
    using StepBase = detail::ServerWriteReactorStepBase;
    using DoneBase = detail::ServerWriteReactorDoneBase;

  public:
    explicit ServerWriteReactor(agrpc::GrpcContext& grpc_context)
        : detail::ServerWriteReactorStepBase(&ServerWriteReactor::handle_step),
          detail::ServerWriteReactorDoneBase(&ServerWriteReactor::handle_done),
          grpc_context(grpc_context)
    {
    }

    template <class... Args>
    static auto create(agrpc::GrpcContext& grpc_context, Args&&... args)
    {
        return detail::allocate<Derived>(grpc_context.get_allocator(), static_cast<Args&&>(args)...).release();
    }

    template <class RPC, class Service, class Request>
    void initiate_request(RPC rpc, Service& service, Request& request, void* tag)
    {
        server_context.AsyncNotifyWhenDone(static_cast<DoneBase*>(this));
        auto* const cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &request, &writer, cq, cq, tag);
    }

    [[nodiscard]] bool is_writing() const noexcept { return write_pending; }

    void write(const Response& response)
    {
        write_pending = true;
        writer.Write(response, static_cast<StepBase*>(this));
    }

    [[nodiscard]] bool is_finished() const noexcept { return finish_called; }

    void finish(const grpc::Status& status)
    {
        finish_called = true;
        writer.Finish(status, static_cast<StepBase*>(this));
    }

    void deallocate() { detail::destroy_deallocate(static_cast<Derived*>(this), get_allocator()); }

  private:
    agrpc::GrpcContext::allocator_type get_allocator() const noexcept { return grpc_context.get_allocator(); }

    static void handle_step(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok, agrpc::GrpcContext&)
    {
        auto* self = static_cast<Derived*>(static_cast<StepBase*>(op));
        self->grpc_context.work_started();
        detail::AllocationGuard guard{self, self->get_allocator()};
        if (std::exchange(self->write_pending, false))
        {
            if (!self->completed)
            {
                guard.release();
            }
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                self->on_write_done(ok);
            }
            if (self->finish_called)
            {
                guard.release();
            }
            return;
        }
        if (self->finish_called)
        {
            if (!self->completed)
            {
                self->completed = true;
                guard.release();
            }
            else if (detail::InvokeHandler::YES == invoke_handler)
            {
                self->on_done();
            }
        }
    }

    static void handle_done(GrpcBase* op, detail::InvokeHandler invoke_handler, bool, agrpc::GrpcContext&)
    {
        auto* self = static_cast<Derived*>(static_cast<DoneBase*>(op));
        self->grpc_context.work_started();
        const auto completed = std::exchange(self->completed, true);
        if (completed || (!self->finish_called && !self->write_pending))
        {
            detail::AllocationGuard guard{self, self->get_allocator()};
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                self->on_done();
            }
        }
    }

    agrpc::GrpcContext& grpc_context;
    grpc::ServerContext server_context;
    grpc::ServerAsyncWriter<Response> writer{&server_context};
    bool write_pending{};
    bool finish_called{};
    bool completed{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_WRITE_REACTOR_HPP
