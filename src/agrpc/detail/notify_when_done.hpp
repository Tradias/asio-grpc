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
struct NotifyWhenDoneSenderImplementation : public detail::IntrusiveListHook<NotifyWhenDoneSenderImplementation>
{
    static constexpr auto TYPE = detail::SenderImplementationType::BOTH;
    static constexpr bool NEEDS_ON_COMPLETE = true;

    using Signature = void();
    using StopFunction = detail::Empty;

    explicit NotifyWhenDoneSenderImplementation(grpc::ServerContext& server_context) noexcept
        : server_context_(server_context)
    {
    }

    template <template <int> class OnComplete>
    void complete(OnComplete<0> on_complete, bool)
    {
        detail::GrpcContextImplementation::work_started(on_complete.grpc_context());
        initiate_async_notify_when_done<OnComplete<0>::ALLOCATION_TYPE>(on_complete.grpc_context(),
                                                                        on_complete.template self<1>());
    }

    template <template <int> class OnComplete>
    void complete(OnComplete<1> on_complete, bool)
    {
        if constexpr (detail::AllocationType::NONE != OnComplete<1>::ALLOCATION_TYPE)
        {
            detail::GrpcContextImplementation::remove_notify_when_done_operation(on_complete.grpc_context(), this);
        }
        on_complete();
    }

    void complete(detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        operation_->complete(result, grpc_context);
    }

    template <detail::AllocationType AllocType>
    void initiate_async_notify_when_done(agrpc::GrpcContext& grpc_context, detail::OperationBase* self)
    {
        if constexpr (detail::AllocationType::NONE != AllocType)
        {
            detail::GrpcContextImplementation::add_notify_when_done_operation(grpc_context, this);
        }
        server_context_.AsyncNotifyWhenDone(self);
    }

    grpc::ServerContext& server_context_;
    detail::OperationBase* operation_;
};

struct NotifyWhenDoneSenderInitiation
{
    template <class Init>
    static void initiate(Init init, NotifyWhenDoneSenderImplementation& impl)
    {
        auto& grpc_context = init.grpc_context();
        if constexpr (detail::AllocationType::NONE == Init::ALLOCATION_TYPE ||
                      detail::AllocationType::LOCAL == Init::ALLOCATION_TYPE)
        {
            impl.operation_ = init.template self<1>();
            impl.initiate_async_notify_when_done<Init::ALLOCATION_TYPE>(grpc_context, impl.operation_);
        }
        else
        {
            if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
            {
                impl.operation_ = init.template self<1>();
                impl.initiate_async_notify_when_done<Init::ALLOCATION_TYPE>(grpc_context, impl.operation_);
            }
            else
            {
                auto* const self = init.template self<0>();
                impl.operation_ = self;
                detail::GrpcContextImplementation::add_remote_operation(grpc_context, self);
            }
        }
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NOTIFY_WHEN_DONE_HPP
