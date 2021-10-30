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

#ifndef AGRPC_DETAIL_INITIATE_HPP
#define AGRPC_DETAIL_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/attributes.hpp"
#include "agrpc/detail/completionHandlerWithPayload.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/grpcContextInteraction.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc::detail
{
template <class Executor>
decltype(auto) query_grpc_context(const Executor& executor)
{
    return static_cast<agrpc::GrpcContext&>(asio::query(executor, asio::execution::context));
}

template <class Function>
struct GrpcInitiator
{
    using executor_type = asio::associated_executor_t<Function>;
    using allocator_type = asio::associated_allocator_t<Function>;

    Function function;

    explicit GrpcInitiator(Function function) : function(std::move(function)) {}

    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler)
    {
        const auto [executor, allocator] = detail::get_associated_executor_and_allocator(completion_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        if (grpc_context.is_stopped()) AGRPC_UNLIKELY
            {
                return;
            }
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
        {
            auto operation =
                detail::allocate_operation<false, void(bool)>(grpc_context, std::move(completion_handler), allocator);
            std::move(this->function)(grpc_context, operation.get());
            operation.release();
        }
        else
        {
            auto operation = detail::allocate_operation<false, void(bool), detail::GrpcContextLocalAllocator>(
                std::move(completion_handler), allocator);
            std::move(this->function)(grpc_context, operation.get());
            operation.release();
        }
        on_exit.release();
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return asio::get_associated_executor(this->function); }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(this->function);
    }
};

template <class Payload, class Function>
struct GrpcWithPayloadInitiator : detail::GrpcInitiator<Function>
{
    using detail::GrpcInitiator<Function>::GrpcInitiator;

    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler)
    {
        detail::GrpcInitiator<Function>::operator()(
            detail::make_completion_handler_with_payload<Payload>(std::move(completion_handler)));
    }
};

template <class Payload, class Function, class CompletionToken>
auto grpc_initiate_with_payload(Function function, CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void(std::pair<Payload, bool>)>(
        detail::GrpcWithPayloadInitiator<Payload, Function>{std::move(function)}, token);
}

struct DefaultCompletionTokenNotAvailable
{
    DefaultCompletionTokenNotAvailable() = delete;
};
}  // namespace agrpc::detail

#ifdef AGRPC_STANDALONE_ASIO
namespace asio
{
template <class Signature>
class async_result<::agrpc::detail::DefaultCompletionTokenNotAvailable, Signature>
{
};
}  // namespace asio
#else
namespace boost::asio
{
template <class Signature>
class async_result<::agrpc::detail::DefaultCompletionTokenNotAvailable, Signature>
{
};
}  // namespace boost::asio
#endif

#endif  // AGRPC_DETAIL_INITIATE_HPP
