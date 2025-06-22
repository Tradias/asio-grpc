// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_HELPER_YIELD_HELPER_HPP
#define AGRPC_HELPER_YIELD_HELPER_HPP

#include "rethrow_first_arg.hpp"

#include <agrpc/grpc_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/spawn.hpp>

#include <tuple>

namespace example
{
template <class Executor, class Function>
void spawn(Executor&& executor, Function&& function)
{
#if (BOOST_VERSION >= 108000)
    boost::asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function), example::RethrowFirstArg{});
#else
    boost::asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function));
#endif
}

template <class Executor, class Function, class CompletionToken>
auto initiate_spawn(Executor&& executor, Function&& function, CompletionToken&& token)
{
#if (BOOST_VERSION >= 108000)
    return boost::asio::spawn(std::forward<Executor>(executor), std::forward<Function>(function),
                              std::forward<CompletionToken>(token));
#else
    auto bound_token =
        boost::asio::bind_executor(std::forward<Executor>(executor), std::forward<CompletionToken>(token));
    return boost::asio::async_initiate<decltype(bound_token), void()>(
        [&](auto&& completion_handler, auto&& f)
        {
            boost::asio::spawn(std::move(completion_handler), std::forward<decltype(f)>(f));
        },
        bound_token, std::forward<Function>(function));
#endif
}

template <class... Function>
struct SpawnAllVoid
{
    std::tuple<Function...> functions;

    explicit SpawnAllVoid(Function... function) : functions(std::move(function)...) {}

    template <class Self>
    void operator()(Self& self)
    {
        std::apply(
            [&](Function&... function)
            {
                const auto executor = boost::asio::get_associated_executor(self);
                boost::asio::experimental::make_parallel_group(
                    [&](auto& f)
                    {
                        return [&](auto&& t)
                        {
                            return example::initiate_spawn(
                                executor,
                                [f = std::move(f)](const auto& yield) mutable
                                {
                                    auto local_f = std::move(f);
                                    std::move(local_f)(yield);
                                },
                                std::forward<decltype(t)>(t));
                        };
                    }(function)...)
                    .async_wait(boost::asio::experimental::wait_for_all(), std::move(self));
            },
            functions);
    }

    template <class Self, class... T>
    void operator()(Self& self, T&&... t)
    {
        (example::RethrowFirstArg{}(t), ...);
        self.complete();
    }
};

template <class CompletionToken, class... Function>
auto spawn_all_void(agrpc::GrpcContext& grpc_context, CompletionToken&& token, Function... function)
{
    return boost::asio::async_compose<CompletionToken, void()>(SpawnAllVoid{std::move(function)...}, token,
                                                               grpc_context);
}
}  // namespace example

#endif  // AGRPC_HELPER_YIELD_HELPER_HPP
