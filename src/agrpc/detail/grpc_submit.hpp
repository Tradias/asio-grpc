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

#ifndef AGRPC_DETAIL_GRPC_SUBMIT_HPP
#define AGRPC_DETAIL_GRPC_SUBMIT_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class InitiatingFunction, class CompletionHandler, class Allocator>
void grpc_submit(agrpc::GrpcContext& grpc_context, InitiatingFunction& initiating_function,
                 CompletionHandler&& completion_handler, const Allocator& allocator)
{
    detail::allocate_operation_and_invoke<false, void(bool)>(
        grpc_context, detail::GrpcContextImplementation::running_in_this_thread(grpc_context),
        std::forward<CompletionHandler>(completion_handler), initiating_function, initiating_function, allocator);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SUBMIT_HPP
