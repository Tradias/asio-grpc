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

#ifndef AGRPC_HELPER_YIELD_HELPER_HPP
#define AGRPC_HELPER_YIELD_HELPER_HPP

#include <agrpc/grpc_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/spawn.hpp>

#include <tuple>

namespace example
{
template <class Function, class CompletionToken>
auto initiate_spawn(Function&& function, CompletionToken&& token)
{
    return boost::asio::async_initiate<CompletionToken, void()>(
        [&](auto&& completion_handler, auto&& f)
        {
            boost::asio::spawn(std::move(completion_handler), std::forward<decltype(f)>(f));
        },
        token, std::forward<Function>(function));
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
                                [f = std::move(f)](const auto& yield) mutable
                                {
                                    auto local_f = std::move(f);
                                    std::move(local_f)(yield);
                                },
                                boost::asio::bind_executor(executor, t));
                        };
                    }(function)...)
                    .async_wait(boost::asio::experimental::wait_for_all(), std::move(self));
            },
            functions);
    }

    template <class Self, std::size_t N>
    void operator()(Self& self, const std::array<std::size_t, N>&)
    {
        self.complete();
    }
};

template <class CompletionToken, class... Function>
auto spawn_all_void(agrpc::GrpcContext& grpc_context, CompletionToken&& token, Function... function)
{
    return boost::asio::async_compose<CompletionToken, void()>(SpawnAllVoid{std::move(function)...}, token,
                                                               grpc_context);
}

template <class Executor, class CompletionToken, class... Function>
auto when_all_bind_executor(const Executor& executor, CompletionToken&& token, Function&&... function)
{
    return boost::asio::experimental::make_parallel_group(
               [&](auto& f)
               {
                   return [&](auto&& t)
                   {
                       return f(boost::asio::bind_executor(executor, std::move(t)));
                   };
               }(function)...)
        .async_wait(boost::asio::experimental::wait_for_all(), std::forward<CompletionToken>(token));
}
}  // namespace example

#endif  // AGRPC_HELPER_YIELD_HELPER_HPP
