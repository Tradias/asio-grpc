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

#ifndef AGRPC_DETAIL_ASYNC_NOTIFY_WHEN_DONE_HPP
#define AGRPC_DETAIL_ASYNC_NOTIFY_WHEN_DONE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class AsyncNotfiyWhenDoneSenderImplementation
{
  public:
    static constexpr auto TYPE = detail::SenderImplementationType::BOTH;

    using Signature = void();
    using StopFunction = detail::Empty;
    using Initiation = detail::Empty;

  private:
    using Self = detail::BasicSenderRunningOperationBase<TYPE>;

  public:
    AsyncNotfiyWhenDoneSenderImplementation(grpc::ServerContext& server_context) : server_context(server_context) {}

    void initiate(agrpc::GrpcContext& grpc_context, const Initiation&, Self* self)
    {
        operation = self;
        if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
        {
            init(grpc_context, self);
        }
        else
        {
            detail::GrpcContextImplementation::work_started(grpc_context);
            detail::GrpcContextImplementation::add_remote_operation(grpc_context, self);
        }
    }

    template <class OnDone>
    void done(OnDone on_done)
    {
        init(on_done.grpc_context(), on_done.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        detail::GrpcContextImplementation::remove_async_notify_when_done_operation(on_done.grpc_context(), this);
        on_done();
    }

    void complete(detail::InvokeHandler invoke_handler, agrpc::GrpcContext& grpc_context)
    {
        operation->complete(invoke_handler, grpc_context);
    }

    AsyncNotfiyWhenDoneSenderImplementation* next;
    AsyncNotfiyWhenDoneSenderImplementation* prev;

  private:
    void init(agrpc::GrpcContext& grpc_context, detail::TypeErasedGrpcTagOperation* self)
    {
        detail::GrpcContextImplementation::add_async_notify_when_done_operation(grpc_context, this);
        server_context.AsyncNotifyWhenDone(self);
    }

    grpc::ServerContext& server_context;
    detail::TypeErasedNoArgOperation* operation;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASYNC_NOTIFY_WHEN_DONE_HPP
