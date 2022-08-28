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

#ifndef AGRPC_DETAIL_REPEATEDLY_REQUEST_HPP
#define AGRPC_DETAIL_REPEATEDLY_REQUEST_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/repeatedly_request_context.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
#include <agrpc/detail/repeatedly_request_awaitable.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
class RepeatedlyRequestStopFunction
{
  public:
    explicit RepeatedlyRequestStopFunction(std::atomic_bool& stopped) noexcept : stopped(stopped) {}

    void operator()(asio::cancellation_type type) noexcept
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            stopped.store(true, std::memory_order_relaxed);
        }
    }

  private:
    std::atomic_bool& stopped;
};
#endif

template <class Operation>
void initiate_repeatedly_request(agrpc::GrpcContext& grpc_context, Operation& operation)
{
    if (!operation.initiate_repeatedly_request())
    {
        detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
    }
}

template <template <class, class, class> class Operation>
struct BasicRepeatedlyRequestInitiator
{
    template <class RequestHandler, class RPC, class CompletionHandler>
    void operator()(CompletionHandler&& completion_handler, RequestHandler&& request_handler, RPC rpc,
                    detail::GetServiceT<RPC>& service) const
    {
        using TrackingCompletionHandler = detail::WorkTrackingCompletionHandler<CompletionHandler>;
        using DecayedRequestHandler = detail::RemoveCrefT<RequestHandler>;
        const auto executor = detail::exec::get_executor(request_handler);
        const auto allocator = detail::exec::get_allocator(request_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        detail::StartWorkAndGuard guard{grpc_context};
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if (auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
            cancellation_slot.is_connected())
        {
            auto operation = detail::allocate<Operation<DecayedRequestHandler, RPC, TrackingCompletionHandler>>(
                allocator, static_cast<RequestHandler&&>(request_handler), rpc, service,
                static_cast<CompletionHandler&&>(completion_handler), true);
            cancellation_slot.template emplace<detail::RepeatedlyRequestStopFunction>(operation->stop_context());
            detail::initiate_repeatedly_request(grpc_context, *operation);
            operation.release();
        }
        else
#endif
        {
            auto operation = detail::allocate<Operation<DecayedRequestHandler, RPC, TrackingCompletionHandler>>(
                allocator, static_cast<RequestHandler&&>(request_handler), rpc, service,
                static_cast<CompletionHandler&&>(completion_handler), false);
            detail::initiate_repeatedly_request(grpc_context, *operation);
            operation.release();
        }
        guard.release();
    }
};

using RepeatedlyRequestInitiator = detail::BasicRepeatedlyRequestInitiator<detail::RepeatedlyRequestOperation>;

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
using RepeatedlyRequestCoroutineInitiator =
    detail::BasicRepeatedlyRequestInitiator<detail::RepeatedlyRequestCoroutineOperation>;
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLY_REQUEST_HPP
