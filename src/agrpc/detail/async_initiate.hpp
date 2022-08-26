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

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
struct InitiateImmediateCompletion;

template <bool>
struct SelectInvokeWithArgs
{
    template <class CompletionHandler, class... Args>
    struct Type
    {
        explicit Type(CompletionHandler&& ch) : ch(static_cast<CompletionHandler&&>(ch)) {}

        void operator()() { static_cast<CompletionHandler&&>(ch)(Args{}...); }

        CompletionHandler ch;
    };
};

template <>
struct SelectInvokeWithArgs<false>
{
    template <class CompletionHandler, class... Args>
    struct Type
    {
        using ArgsTuple = detail::Tuple<Args...>;

        template <class... U>
        explicit Type(CompletionHandler&& ch, U&&... u)
            : ch(static_cast<CompletionHandler&&>(ch)), args{static_cast<U&&>(u)...}
        {
        }

        void operator()() { detail::apply(static_cast<CompletionHandler&&>(ch), static_cast<ArgsTuple&&>(args)); }

        CompletionHandler ch;
        ArgsTuple args;
    };
};

template <class... Args>
struct InitiateImmediateCompletion<void(Args...)>
{
    template <class CompletionHandler, class... T>
    void operator()(CompletionHandler&& ch, T&&... t) const
    {
        auto executor = asio::get_associated_executor(ch);
        const auto allocator = asio::get_associated_allocator(ch);
        using Invoker = typename detail::SelectInvokeWithArgs<(
            0 == sizeof...(T))>::template Type<detail::RemoveCrefT<CompletionHandler>, Args...>;
        detail::post_with_allocator(std::move(executor),
                                    Invoker{static_cast<CompletionHandler&&>(ch), static_cast<T&&>(t)...}, allocator);
    }
};

template <class Signature, class CompletionToken, class... Args>
auto async_initiate_immediate_completion(CompletionToken token, Args&&... args)
{
    return asio::async_initiate<CompletionToken, Signature>(detail::InitiateImmediateCompletion<Signature>{}, token,
                                                            static_cast<Args&&>(args)...);
}
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_ASYNC_INITIATE_HPP
