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
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"
#include "agrpc/grpcExecutor.hpp"
#include "agrpc/grpcSender.hpp"

namespace agrpc
{
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::GrpcExecutor>;

using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::GrpcExecutor>;

static constexpr agrpc::GrpcUseAwaitable GRPC_USE_AWAITABLE{};

namespace pmr
{
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::pmr::GrpcExecutor>;

using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::pmr::GrpcExecutor>;

static constexpr agrpc::pmr::GrpcUseAwaitable GRPC_USE_AWAITABLE{};
}  // namespace pmr

using DefaultCompletionToken = asio::use_awaitable_t<>;
#else
using DefaultCompletionToken = detail::DefaultCompletionTokenNotAvailable;
#endif

namespace detail
{
template <class Scheduler>
struct UseScheduler
{
    Scheduler scheduler;
};

template <class Scheduler>
UseScheduler(Scheduler&&) -> UseScheduler<Scheduler>;

struct UseSchedulerFn
{
    template <class Scheduler>
    auto operator()(Scheduler&& scheduler) const noexcept
    {
        return detail::UseScheduler{std::forward<Scheduler>(scheduler)};
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    auto operator()(asio::execution_context& context) const noexcept
    {
        return detail::UseScheduler{static_cast<agrpc::GrpcContext&>(context).get_scheduler()};
    }
#endif

    auto operator()(agrpc::GrpcContext& context) const noexcept
    {
        return detail::UseScheduler{context.get_scheduler()};
    }
};
}  // namespace detail

inline constexpr detail::UseSchedulerFn use_scheduler{};

template <class Allocator, std::uint32_t Options>
[[nodiscard]] auto get_completion_queue(const agrpc::BasicGrpcExecutor<Allocator, Options>& executor) noexcept
{
    return executor.context().get_completion_queue();
}

[[nodiscard]] inline auto get_completion_queue(agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.get_completion_queue();
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
[[nodiscard]] inline auto get_completion_queue(const asio::any_io_executor& executor) noexcept
{
    return detail::query_grpc_context(executor).get_completion_queue();
}

template <class CompletionToken>
[[nodiscard]] auto get_completion_queue(const CompletionToken& token) noexcept
{
    const auto executor = asio::get_associated_executor(token);
    return agrpc::get_completion_queue(executor);
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class Executor = asio::any_io_executor>
[[nodiscard]] auto get_completion_queue(asio::use_awaitable_t<Executor> = {}) ->
    typename asio::async_result<asio::use_awaitable_t<Executor>, void(grpc::CompletionQueue*)>::return_type
{
    const auto executor = co_await asio::this_coro::executor;
    co_return detail::query_grpc_context(executor).get_completion_queue();
}
#endif

template <class Function, class CompletionToken = agrpc::DefaultCompletionToken>
auto grpc_initiate(Function function, CompletionToken token = {})
{
    return asio::async_initiate<CompletionToken, void(bool)>(detail::GrpcInitiator{std::move(function)}, token);
}
#endif

template <class Function, class Scheduler>
auto grpc_initiate(Function function, detail::UseScheduler<Scheduler> token)
{
    return agrpc::GrpcSender{static_cast<agrpc::GrpcContext&>(token.scheduler.context()), std::move(function)};
}
}  // namespace agrpc

#endif  // AGRPC_AGRPC_INITIATE_HPP
