// Copyright 2023 Dennis Hezel
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
        : StepBase(nullptr), DoneBase(&ServerWriteReactor::do_done_notified), grpc_context_(grpc_context)
    {
        server_context_.AsyncNotifyWhenDone(static_cast<DoneBase*>(this));
        auto* const cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context_, &request, &writer_, cq, cq, tag);
    }

    template <class... Args>
    static auto create(agrpc::GrpcContext& grpc_context, Args&&... args)
    {
        return detail::allocate<Derived>(grpc_context.get_allocator(), static_cast<Args&&>(args)...).release();
    }

    [[nodiscard]] bool is_writing() const noexcept { return &ServerWriteReactor::do_write_done == get_on_complete(); }

    void write(const Response& response)
    {
        set_on_complete(&ServerWriteReactor::do_write_done);
        writer_.Write(response, static_cast<StepBase*>(this));
    }

    [[nodiscard]] bool is_finishing() const noexcept
    {
        return &ServerWriteReactor::do_finish_done == get_on_complete();
    }

    void finish(const grpc::Status& status)
    {
        set_on_complete(&ServerWriteReactor::do_finish_done);
        writer_.Finish(status, static_cast<StepBase*>(this));
    }

    void deallocate() { detail::destroy_deallocate(static_cast<Derived*>(this), get_allocator()); }

  private:
    auto get_allocator() const noexcept { return grpc_context_.get_allocator(); }

    auto get_on_complete() const noexcept
    {
        return detail::OperationBaseAccess::get_on_complete(*static_cast<const StepBase*>(this));
    }

    void set_on_complete(detail::OperationOnComplete on_complete) noexcept
    {
        detail::OperationBaseAccess::set_on_complete(*static_cast<StepBase*>(this), on_complete);
    }

    void set_step_done() noexcept { set_on_complete(nullptr); }

    bool is_finishing_or_writing() const noexcept { return nullptr != get_on_complete(); }

    auto get_on_done() const noexcept
    {
        return detail::OperationBaseAccess::get_on_complete(*static_cast<const DoneBase*>(this));
    }

    void set_on_done(detail::OperationOnComplete on_complete) noexcept
    {
        detail::OperationBaseAccess::set_on_complete(*static_cast<DoneBase*>(this), on_complete);
    }

    bool is_completed() const noexcept { return nullptr == get_on_done(); }

    void set_completed() noexcept { set_on_done(nullptr); }

    static void do_write_done(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* const self = static_cast<ServerWriteReactor*>(static_cast<StepBase*>(op));
        self->set_step_done();
        self->grpc_context_.work_started();
        detail::AllocationGuard guard{static_cast<Derived*>(self), self->get_allocator()};
        const bool completed = self->is_completed();
        if (!completed)
        {
            guard.release();
        }
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            static_cast<Derived*>(self)->on_write_done(detail::is_ok(result));
            if AGRPC_UNLIKELY (completed)
            {
                static_cast<Derived*>(self)->on_done();
            }
        }
        if (self->is_finishing())
        {
            guard.release();
        }
    }

    static void do_finish_done(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* const self = static_cast<ServerWriteReactor*>(static_cast<StepBase*>(op));
        self->set_step_done();
        self->grpc_context_.work_started();
        detail::AllocationGuard guard{static_cast<Derived*>(self), self->get_allocator()};
        if (!self->is_completed())
        {
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
        self->set_completed();
        self->grpc_context_.work_started();
        if (!self->is_finishing_or_writing())
        {
            detail::AllocationGuard guard{static_cast<Derived*>(self), self->get_allocator()};
            if AGRPC_LIKELY (!detail::is_shutdown(result))
            {
                static_cast<Derived*>(self)->on_done();
            }
        }
    }

    agrpc::GrpcContext& grpc_context_;
    grpc::ServerContext server_context_;
    grpc::ServerAsyncWriter<Response> writer_{&server_context_};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_WRITE_REACTOR_HPP
