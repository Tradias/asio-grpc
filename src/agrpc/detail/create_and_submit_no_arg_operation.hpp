// Copyright 2024 Dennis Hezel
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

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/no_arg_operation.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
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
    auto operation = detail::allocate_operation<detail::NoArgOperation>(static_cast<Handler&&>(handler));
    GrpcContextImplementation::work_started(grpc_context);
    if (is_running_in_this_thread)
    {
        detail::GrpcContextImplementation::add_local_operation(operation);
    }
    else
    {
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, operation);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CREATE_AND_SUBMIT_NO_ARG_OPERATION_HPP
