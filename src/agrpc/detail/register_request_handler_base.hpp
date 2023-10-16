// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_REGISTER_REQUEST_HANDLER_BASE_HPP
#define AGRPC_DETAIL_REGISTER_REQUEST_HANDLER_BASE_HPP

#include <agrpc/detail/atomic_bool_stop_context.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class RegisterRequestHandlerOperationComplete
{
  public:
    using Complete = void (*)(RegisterRequestHandlerOperationComplete&) noexcept;

    explicit RegisterRequestHandlerOperationComplete(Complete complete) noexcept : complete_(complete) {}

    void complete() noexcept { complete_(*this); }

  private:
    Complete complete_;
};

template <class ServerRPC, class RequestHandler, class StopToken>
struct RegisterRequestHandlerOperationBase
{
    using Service = detail::GetServerRPCServiceT<ServerRPC>;
    using ServerRPCExecutor = typename ServerRPC::executor_type;

    RegisterRequestHandlerOperationBase(const ServerRPCExecutor& executor, Service& service,
                                        RequestHandler&& request_handler)
        : executor_(executor), service_(service), request_handler_(static_cast<RequestHandler&&>(request_handler))
    {
    }

    bool is_stopped() const noexcept
    {
        return stop_context_.is_stopped() || has_error_.load(std::memory_order_relaxed);
    }

    void stop() noexcept { stop_context_.stop(); }

    agrpc::GrpcContext& grpc_context() const noexcept { return detail::query_grpc_context(executor_); }

    const ServerRPCExecutor& get_executor() const noexcept { return executor_; }

    Service& service() const noexcept { return service_; }

    RequestHandler& request_handler() noexcept { return request_handler_; }

    void set_error(std::exception_ptr&& eptr) noexcept
    {
        if (!has_error_.exchange(true))
        {
            eptr_ = static_cast<std::exception_ptr&&>(eptr);
        }
    }

    std::exception_ptr& error() noexcept { return eptr_; }

    void increment_ref_count() noexcept { ++reference_count_; }

    [[nodiscard]] bool decrement_ref_count() noexcept { return 0 == --reference_count_; }

    ServerRPCExecutor executor_;
    Service& service_;
    std::atomic_size_t reference_count_{};
    std::exception_ptr eptr_{};
    std::atomic_bool has_error_{};
    detail::AtomicBoolStopContext<StopToken> stop_context_;
    RequestHandler request_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_REQUEST_HANDLER_BASE_HPP
