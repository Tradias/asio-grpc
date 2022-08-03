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
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/spawn.hpp>

namespace example
{
template <class Function, class Token>
auto async_initiate_void(Function&& init_function, Token& token)
{
    return boost::asio::async_initiate<Token, void()>(std::forward<Function>(init_function), token);
}

template <class Function, class Token>
auto async_initiate_and_spawn(agrpc::GrpcContext& grpc_context, Function&& function, Token& token)
{
    return example::async_initiate_void(
        [&](auto&& completion_handler)
        {
            auto completion_handler_with_executor =
                boost::asio::bind_executor(grpc_context, std::move(completion_handler));
            boost::asio::spawn(std::move(completion_handler_with_executor),
                               [&, f = std::forward<Function>(function)](const auto& yield) mutable
                               {
                                   std::move(f)(yield);
                               });
        },
        token);
}

template <class Handler, class... Function>
void yield_spawn_all(agrpc::GrpcContext& grpc_context, const boost::asio::basic_yield_context<Handler>& yield,
                     Function&&... function)
{
    example::async_initiate_void(
        [&](auto&& ch)
        {
            boost::asio::experimental::make_parallel_group(
                [&](auto& f)
                {
                    return [&](auto&& token)
                    {
                        return example::async_initiate_and_spawn(
                            grpc_context,
                            [&, f = std::forward<Function>(f)](const auto& yield) mutable
                            {
                                std::move(f)(yield);
                            },
                            token);
                    };
                }(function)...)
                .async_wait(boost::asio::experimental::wait_for_all(),
                            [ch = std::move(ch)](auto&&...) mutable
                            {
                                std::move(ch)();
                            });
        },
        yield);
}

template <class Handler, class Completion, class... Function>
void yield_when_all(agrpc::GrpcContext& grpc_context, const boost::asio::basic_yield_context<Handler>& yield,
                    Completion&& completion, Function&&... function)
{
    example::async_initiate_void(
        [&](auto&& ch)
        {
            boost::asio::experimental::make_parallel_group(
                [&](auto& f)
                {
                    return [&](auto&& token)
                    {
                        return f(boost::asio::bind_executor(grpc_context, std::move(token)));
                    };
                }(function)...)
                .async_wait(boost::asio::experimental::wait_for_all(),
                            [&, ch = std::move(ch)](auto /*completion_order*/, auto&&... result) mutable
                            {
                                completion(result...);
                                std::move(ch)();
                            });
        },
        yield);
}
}  // namespace example

#endif  // AGRPC_HELPER_YIELD_HELPER_HPP
