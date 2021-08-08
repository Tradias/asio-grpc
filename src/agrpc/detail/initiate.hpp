// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_INITIATE_HPP
#define AGRPC_DETAIL_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc::detail
{
template <class CompletionHandler, class Function>
auto create_work_and_invoke(CompletionHandler completion_handler, Function&& function)
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(completion_handler);
    using Querier = detail::CanQuery<decltype(executor), asio::execution::context_t>;
    auto&& grpc_context = static_cast<agrpc::GrpcContext&>(Querier::query(executor, asio::execution::context_t{}));
    detail::create_work<true>(
        grpc_context, std::move(completion_handler),
        [&](auto* work)
        {
            function(grpc_context, work);
        },
        allocator);
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_INITIATE_HPP
