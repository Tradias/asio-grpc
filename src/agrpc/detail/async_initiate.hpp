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

#ifndef AGRPC_DETAIL_ASYNC_INITIATE_HPP
#define AGRPC_DETAIL_ASYNC_INITIATE_HPP

#include "agrpc/detail/asio_forward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/tuple.hpp"
#include "agrpc/detail/utility.hpp"

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
struct InitiateImmediateCompletion;

template <class... Args>
struct InitiateImmediateCompletion<void(Args...)>
{
    template <class CompletionHandler, class... T>
    void operator()(CompletionHandler&& ch, T&&... t) const
    {
        auto executor = asio::get_associated_executor(ch);
        const auto allocator = asio::get_associated_allocator(ch);
        detail::post_with_allocator(
            std::move(executor),
            [impl = detail::CompressedPair(std::forward<CompletionHandler>(ch),
                                           detail::Tuple{std::forward<T>(t)...})]() mutable
            {
                if constexpr (0 == sizeof...(T))
                {
                    std::move(impl.first())(Args{}...);
                }
                else
                {
                    detail::apply(std::move(impl.first()), std::move(impl.second()));
                }
            },
            allocator);
    }
};

template <class Signature, class CompletionToken, class... Args>
auto async_initiate_immediate_completion(CompletionToken token, Args&&... args)
{
    return asio::async_initiate<CompletionToken, Signature>(detail::InitiateImmediateCompletion<Signature>{}, token,
                                                            std::forward<Args>(args)...);
}
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_ASYNC_INITIATE_HPP
