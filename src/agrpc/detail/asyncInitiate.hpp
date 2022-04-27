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

#ifndef AGRPC_DETAIL_ASYNCINITIATE_HPP
#define AGRPC_DETAIL_ASYNCINITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
struct InitiateImmediateCompletion;

template <class R, class... Args>
struct InitiateImmediateCompletion<R(Args...)>
{
    template <class CompletionHandler>
    void operator()(CompletionHandler&& ch)
    {
        auto executor = asio::get_associated_executor(ch);
        const auto allocator = asio::get_associated_allocator(ch);
        detail::post_with_allocator(
            std::move(executor),
            [ch = std::decay_t<CompletionHandler>{std::forward<CompletionHandler>(ch)}]() mutable
            {
                std::move(ch)(Args{}...);
            },
            allocator);
    }
};

template <class Signature, class CompletionToken>
auto async_initiate_immediate_completion(CompletionToken&& token)
{
    return asio::async_initiate<CompletionToken, Signature>(detail::InitiateImmediateCompletion<Signature>{}, token);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASYNCINITIATE_HPP
