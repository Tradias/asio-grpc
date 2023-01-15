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

#include <agrpc/detail/asio_association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/tuple.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler, bool DefaultConstructArgs, class... Args>
struct InvokeWithArgs
{
    void operator()() { static_cast<CompletionHandler&&>(ch_)(Args{}...); }

    CompletionHandler ch_;
};

template <class CompletionHandler, class... Args>
struct InvokeWithArgs<CompletionHandler, false, Args...>
{
    using ArgsTuple = detail::Tuple<Args...>;

    template <class... U>
    explicit InvokeWithArgs(CompletionHandler&& ch, U&&... u)
        : ch_(static_cast<CompletionHandler&&>(ch)), args_{static_cast<U&&>(u)...}
    {
    }

    void operator()() { detail::apply(static_cast<CompletionHandler&&>(ch_), static_cast<ArgsTuple&&>(args_)); }

    CompletionHandler ch_;
    ArgsTuple args_;
};

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
        using Invoker = detail::InvokeWithArgs<detail::RemoveCrefT<CompletionHandler>, (0 == sizeof...(T)), Args...>;
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
