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
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/detail/tagged_ptr.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct ServerWriteReactorStepBase : detail::OperationBase
{
    using detail::OperationBase::OperationBase;
};

struct ServerWriteReactorDoneBase : detail::OperationBase
{
    using detail::OperationBase::OperationBase;
};

template <class Derived, class Response>
class ServerWriteReactor : public detail::ServerWriteReactorStepBase, public detail::ServerWriteReactorDoneBase
{
  private:
    using StepBase = detail::ServerWriteReactorStepBase;
    using DoneBase = detail::ServerWriteReactorDoneBase;

  public:
    template <class RPC, class Service, class Request>
    ServerWriteReactor(agrpc::GrpcContext& grpc_context, RPC rpc, Service& service, Request& request, void* tag)
        : detail::ServerWriteReactorStepBase(nullptr),
          detail::ServerWriteReactorDoneBase(&ServerWriteReactor::do_done_notified),
          grpc_context(&grpc_context)
    {
        server_context.AsyncNotifyWhenDone(static_cast<DoneBase*>(this));
        auto* const cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &request, &writer, cq, cq, tag);
    }

    template <class... Args>
    static auto create(agrpc::GrpcContext& grpc_context, Args&&... args)
    {
        return detail::allocate<Derived>(grpc_context.get_allocator(), static_cast<Args&&>(args)...).release();
    }

    [[nodiscard]] bool is_writing() const noexcept { return &ServerWriteReactor::do_write_done == get_on_complete(); }

    void write(const Response& response)
    {
        get_on_complete() = &ServerWriteReactor::do_write_done;
        writer.Write(response, static_cast<StepBase*>(this));
    }

    [[nodiscard]] bool is_finished() const noexcept { return &ServerWriteReactor::do_finish_done == get_on_complete(); }

    void finish(const grpc::Status& status)
    {
        get_on_complete() = &ServerWriteReactor::do_finish_done;
        writer.Finish(status, static_cast<StepBase*>(this));
    }

    void deallocate() { detail::destroy_deallocate(static_cast<Derived*>(this), get_allocator()); }

  private:
    auto get_allocator() const noexcept { return grpc_context->get_allocator(); }

    auto& get_on_complete() noexcept
    {
        return detail::OperationBaseAccess::get_on_complete(*static_cast<StepBase*>(this));
    }

    auto get_on_complete() const noexcept
    {
        return detail::OperationBaseAccess::get_on_complete(*static_cast<const StepBase*>(this));
    }

    void set_step_done() noexcept { get_on_complete() = nullptr; }

    bool is_finishing_or_writing() const noexcept { return nullptr != get_on_complete(); }

    bool is_completed() const noexcept { return grpc_context.has_bit<0>(); }

    bool set_completed() noexcept
    {
        const auto old_value = is_completed();
        grpc_context.set_bit<0>();
        return old_value;
    }

    static void do_write_done(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* const self = static_cast<ServerWriteReactor*>(static_cast<StepBase*>(op));
        self->set_step_done();
        self->grpc_context->work_started();
        detail::AllocationGuard guard{static_cast<Derived*>(self), self->get_allocator()};
        if (!self->is_completed())
        {
            guard.release();
        }
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            static_cast<Derived*>(self)->on_write_done(detail::is_ok(result));
        }
        if (self->is_finished())
        {
            guard.release();
        }
    }

    static void do_finish_done(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* const self = static_cast<ServerWriteReactor*>(static_cast<StepBase*>(op));
        self->set_step_done();
        self->grpc_context->work_started();
        detail::AllocationGuard guard{static_cast<Derived*>(self), self->get_allocator()};
        if (!self->is_completed())
        {
            self->set_completed();
            guard.release();
        }
        else if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            static_cast<Derived*>(self)->on_done();
        }
    }

    static void do_done_notified(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* const self = static_cast<ServerWriteReactor*>(static_cast<DoneBase*>(op));
        self->grpc_context->work_started();
        const bool completed = self->set_completed();
        if (completed || !self->is_finishing_or_writing())
        {
            detail::AllocationGuard guard{static_cast<Derived*>(self), self->get_allocator()};
            if AGRPC_LIKELY (!detail::is_shutdown(result))
            {
                static_cast<Derived*>(self)->on_done();
            }
        }
    }

    detail::TaggedPtr<agrpc::GrpcContext> grpc_context;
    grpc::ServerContext server_context;
    grpc::ServerAsyncWriter<Response> writer{&server_context};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_WRITE_REACTOR_HPP
