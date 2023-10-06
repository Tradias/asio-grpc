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

#ifndef AGRPC_DETAIL_CREATE_AND_SUBMIT_NO_ARG_OPERATION_HPP
#define AGRPC_DETAIL_CREATE_AND_SUBMIT_NO_ARG_OPERATION_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/schedule_sender.hpp>
#include <agrpc/detail/sender_implementation_operation.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct NoArgOperationInitiation
{
    static void initiate(const agrpc::GrpcContext&, const detail::QueueableOperationBase*) noexcept {}
};

template <bool IsBlockingNever, class Handler>
void create_and_submit_no_arg_operation(agrpc::GrpcContext& grpc_context, Handler&& handler)
{
    if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
    {
        return;
    }
    const auto is_running_in_this_thread = detail::GrpcContextImplementation::running_in_this_thread(grpc_context);
    if constexpr (!IsBlockingNever)
    {
        if (is_running_in_this_thread)
        {
            auto op{static_cast<Handler&&>(handler)};
            std::move(op)();
            return;
        }
    }
    using Template = detail::SenderImplementationOperationTemplate<detail::ScheduleSenderImplementation>;
    if (is_running_in_this_thread)
    {
        auto operation = detail::allocate_local_operation<Template::Type>(
            grpc_context, static_cast<Handler&&>(handler), grpc_context, detail::NoArgOperationInitiation{},
            detail::ScheduleSenderImplementation{});
        detail::GrpcContextImplementation::add_local_operation(grpc_context, operation);
    }
    else
    {
        auto operation = detail::allocate_custom_operation<Template::Type>(
            static_cast<Handler&&>(handler), grpc_context, detail::NoArgOperationInitiation{},
            detail::ScheduleSenderImplementation{});
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, operation);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CREATE_AND_SUBMIT_NO_ARG_OPERATION_HPP
