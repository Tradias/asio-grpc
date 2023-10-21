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

#ifndef AGRPC_DETAIL_RPC_EXECUTOR_BASE_HPP
#define AGRPC_DETAIL_RPC_EXECUTOR_BASE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/tagged_ptr.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief (experimental) RPC's executor base
 *
 * @since 2.1.0
 */
template <class Executor>
class RPCExecutorBase
{
  public:
    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    /**
     * @brief Get the executor
     *
     * Thread-safe
     */
    [[nodiscard]] const executor_type& get_executor() const noexcept { return executor_; }

  private:
    template <auto, class>
    friend class agrpc::ClientRPC;

    template <auto, class, class>
    friend class agrpc::ServerRPC;

    template <class, class>
    friend class detail::ClientRPCBase;

    template <auto, class>
    friend class detail::ClientRPCUnaryBase;

    template <auto, class>
    friend class detail::ClientRPCServerStreamingBase;

    template <class, class>
    friend class detail::ClientRPCBidiStreamingBase;

    template <class, class, class>
    friend class detail::ServerRPCBidiStreamingBase;

    template <bool, class, class>
    friend class detail::ServerRPCNotifyWhenDoneMixin;

    friend detail::RPCExecutorBaseAccess;

    friend detail::ServerRPCContextBaseAccess;

    RPCExecutorBase() : executor_(agrpc::GrpcExecutor{}) {}

    explicit RPCExecutorBase(const Executor& executor) : executor_(executor) {}

    [[nodiscard]] agrpc::GrpcContext& grpc_context() const noexcept { return detail::query_grpc_context(executor_); }

    Executor executor_;
};

struct RPCExecutorBaseAccess
{
    template <class T>
    using DefaultCompletionTokenT = detail::DefaultCompletionTokenT<typename T::executor_type>;

    template <class Executor>
    static agrpc::GrpcContext& grpc_context(detail::RPCExecutorBase<Executor>& rpc) noexcept
    {
        return rpc.grpc_context();
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPC_EXECUTOR_BASE_HPP
