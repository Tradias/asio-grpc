#ifndef AGRPC_AGRPC_INITIATE_HPP
#define AGRPC_AGRPC_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/initiate.hpp"
#include "agrpc/grpcExecutor.hpp"

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
}  // namespace agrpc

#endif  // AGRPC_AGRPC_INITIATE_HPP
