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

#ifndef AGRPC_AGRPC_INITIATE_HPP
#define AGRPC_AGRPC_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/initiate.hpp"
#include "agrpc/grpcExecutor.hpp"

#ifdef __cpp_lib_coroutine
#include <boost/asio/use_awaitable.hpp>
#endif

namespace agrpc
{
#ifdef __cpp_lib_coroutine
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::GrpcExecutor>;

using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::GrpcExecutor>;

static constexpr GrpcUseAwaitable GRPC_USE_AWAITABLE{};

namespace pmr
{
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::pmr::GrpcExecutor>;

static constexpr asio::use_awaitable_t<agrpc::pmr::GrpcExecutor> GRPC_USE_AWAITABLE{};
}  // namespace pmr

using DefaultCompletionToken = asio::use_awaitable_t<>;
#else
using DefaultCompletionToken = asio::use_future_t<>;
#endif

template <class Function, class CompletionToken = agrpc::DefaultCompletionToken>
auto grpc_initiate(Function function, CompletionToken token = {})
{
    return asio::async_initiate<CompletionToken, void(bool)>(
        [function = std::move(function)](auto completion_handler) mutable
        {
            detail::create_work_and_invoke(std::move(completion_handler), std::move(function));
        },
        token);
}

template <class CompletionToken>
auto get_completion_queue(CompletionToken token) noexcept
{
    const auto executor = asio::get_associated_executor(token);
    return static_cast<agrpc::GrpcContext&>(executor.context()).get_completion_queue();
}

#ifdef __cpp_impl_coroutine
template <class Executor = asio::any_io_executor>
auto get_completion_queue(asio::use_awaitable_t<Executor> = {})
    -> asio::async_result<asio::use_awaitable_t<Executor>, void(grpc::CompletionQueue*)>::return_type
{
    const auto executor = co_await asio::this_coro::executor;
    co_return static_cast<agrpc::GrpcContext&>(executor.context()).get_completion_queue();
}
#endif
}  // namespace agrpc

#endif  // AGRPC_AGRPC_INITIATE_HPP
