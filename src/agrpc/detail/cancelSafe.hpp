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

#ifndef AGRPC_DETAIL_CANCELSAFE_HPP
#define AGRPC_DETAIL_CANCELSAFE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include "agrpc/detail/allocate.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler, class... Args>
void deallocate_and_invoke(void* data, Args... args)
{
    using Allocator = typename std::allocator_traits<
        asio::associated_allocator_t<CompletionHandler>>::template rebind_alloc<CompletionHandler>;
    auto* completion_handler = static_cast<CompletionHandler*>(data);
    auto local_completion_handler{std::move(*completion_handler)};
    auto allocator = asio::get_associated_allocator(local_completion_handler);
    detail::deallocate<Allocator>(allocator, completion_handler);
    std::move(local_completion_handler)(std::move(args)...);
}

template <class Executor, class Allocator, class Function>
void post_with_allocator(Executor&& executor, Allocator&& allocator, Function&& function)
{
    asio::execution::execute(
        asio::prefer(asio::require(std::forward<Executor>(executor), asio::execution::blocking.never),
                     asio::execution::relationship.fork,
                     asio::execution::allocator(std::forward<Allocator>(allocator))),
        std::forward<Function>(function));
}

template <class CompletionHandler, class... Args>
void post_and_complete(void* ptr, Args... args)
{
    auto* completion_handler = static_cast<CompletionHandler*>(ptr);
    detail::post_with_allocator(asio::get_associated_executor(*completion_handler),
                                asio::get_associated_allocator(*completion_handler),
                                [ptr, args = std::tuple(std::move(args)...)]() mutable
                                {
                                    std::apply(&detail::deallocate_and_invoke<CompletionHandler, Args...>,
                                               std::tuple_cat(std::tuple(ptr), std::move(args)));
                                });
}
}
AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_CANCELSAFE_HPP
