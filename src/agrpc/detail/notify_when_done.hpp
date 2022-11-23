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

#ifndef AGRPC_DETAIL_NOTIFY_WHEN_DONE_HPP
#define AGRPC_DETAIL_NOTIFY_WHEN_DONE_HPP

#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/intrusive_list_hook.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class NotfiyWhenDoneSenderImplementation : public detail::IntrusiveListHook<NotfiyWhenDoneSenderImplementation>
{
  public:
    static constexpr auto TYPE = detail::SenderImplementationType::BOTH;

    using Signature = void();
    using StopFunction = detail::Empty;
    using Initiation = detail::Empty;

    NotfiyWhenDoneSenderImplementation(grpc::ServerContext& server_context) : server_context(server_context) {}

    template <class Init>
    void initiate(Init init, const Initiation&)
    {
        auto& grpc_context = init.grpc_context();
        if constexpr (detail::AllocationType::NONE == Init::ALLOCATION_TYPE ||
                      detail::AllocationType::LOCAL == Init::ALLOCATION_TYPE)
        {
            operation = init.template self<1>();
            initiate_async_notify_when_done<Init::ALLOCATION_TYPE>(grpc_context, operation);
        }
        else
        {
            if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
            {
                operation = init.template self<1>();
                initiate_async_notify_when_done<Init::ALLOCATION_TYPE>(grpc_context, operation);
            }
            else
            {
                auto* const self = init.template self<0>();
                operation = self;
                detail::GrpcContextImplementation::add_remote_operation(grpc_context, self);
            }
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool)
    {
        detail::GrpcContextImplementation::work_started(on_done.grpc_context());
        initiate_async_notify_when_done<OnDone<0>::ALLOCATION_TYPE>(on_done.grpc_context(), on_done.template self<1>());
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        if constexpr (detail::AllocationType::NONE != OnDone<1>::ALLOCATION_TYPE)
        {
            detail::GrpcContextImplementation::remove_notify_when_done_operation(on_done.grpc_context(), this);
        }
        on_done();
    }

    void complete(detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        operation->complete(result, grpc_context);
    }

  private:
    template <detail::AllocationType AllocType>
    void initiate_async_notify_when_done(agrpc::GrpcContext& grpc_context, detail::OperationBase* self)
    {
        if constexpr (detail::AllocationType::NONE != AllocType)
        {
            detail::GrpcContextImplementation::add_notify_when_done_operation(grpc_context, this);
        }
        server_context.AsyncNotifyWhenDone(self);
    }

    grpc::ServerContext& server_context;
    detail::OperationBase* operation;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NOTIFY_WHEN_DONE_HPP
