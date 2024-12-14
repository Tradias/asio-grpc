// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_REGISTER_RPC_HANDLER_BASE_HPP
#define AGRPC_DETAIL_REGISTER_RPC_HANDLER_BASE_HPP

#include <agrpc/detail/atomic_bool_stop_context.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <atomic>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class RegisterRPCHandlerOperationComplete
{
  public:
    using Complete = void (*)(RegisterRPCHandlerOperationComplete&) noexcept;

    explicit RegisterRPCHandlerOperationComplete(Complete complete) noexcept : complete_(complete) {}

    void complete() noexcept { complete_(*this); }

  private:
    Complete complete_;
};

template <class ServerRPC, class RPCHandler, class StopToken>
struct RegisterRPCHandlerOperationBase : RegisterRPCHandlerOperationComplete
{
    using Service = detail::ServerRPCServiceT<ServerRPC>;
    using ServerRPCExecutor = typename ServerRPC::executor_type;

    RegisterRPCHandlerOperationBase(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler,
                                    RegisterRPCHandlerOperationComplete::Complete complete)
        : RegisterRPCHandlerOperationComplete{complete},
          executor_(executor),
          service_(service),
          rpc_handler_(static_cast<RPCHandler&&>(rpc_handler))
    {
    }

    bool is_stopped() const noexcept
    {
        return stop_context_.is_stopped() || has_error_.load(std::memory_order_relaxed);
    }

    agrpc::GrpcContext& grpc_context() const noexcept { return detail::query_grpc_context(executor_); }

    const ServerRPCExecutor& get_executor() const noexcept { return executor_; }

    Service& service() const noexcept { return service_; }

    RPCHandler& rpc_handler() noexcept { return rpc_handler_; }

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

    void notify_when_done_work_started() noexcept
    {
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            grpc_context().work_started();
        }
    }

    ServerRPCExecutor executor_;
    Service& service_;
    std::atomic_size_t reference_count_{};
    std::exception_ptr eptr_{};
    std::atomic_bool has_error_{};
    detail::AtomicBoolStopContext<StopToken> stop_context_;
    RPCHandler rpc_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_RPC_HANDLER_BASE_HPP
