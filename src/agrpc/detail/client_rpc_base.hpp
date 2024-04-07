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

#ifndef AGRPC_DETAIL_CLIENT_RPC_BASE_HPP
#define AGRPC_DETAIL_CLIENT_RPC_BASE_HPP

#include <agrpc/detail/client_rpc_context_base.hpp>
#include <agrpc/detail/client_rpc_sender.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/rpc_type.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief ServerRPC base
 *
 * @since 2.7.0
 */
template <class Responder, class Executor>
class ClientRPCBase : public detail::RPCExecutorBase<Executor>, public detail::ClientRPCContextBase<Responder>
{
  public:
    /**
     * @brief Construct from a GrpcContext
     */
    explicit ClientRPCBase(agrpc::GrpcContext& grpc_context)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor())
    {
    }

    /**
     * @brief Construct from a GrpcContext and an init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this rpc's deadline.
     */
    template <class ClientContextInitFunction>
    ClientRPCBase(agrpc::GrpcContext& grpc_context, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor()),
          detail::ClientRPCContextBase<Responder>(static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    /**
     * @brief Construct from an executor
     */
    explicit ClientRPCBase(const Executor& executor) : detail::RPCExecutorBase<Executor>(executor) {}

    /**
     * @brief Construct from an executor and init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this rpc's deadline.
     */
    template <class ClientContextInitFunction>
    ClientRPCBase(const Executor& executor, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(executor),
          detail::ClientRPCContextBase<Responder>(static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * @attention If the server does not explicitly send initial metadata (e.g. by calling
     * `agrpc::send_initial_metadata`) but waits for a message from the client instead then this function won't
     * complete until `write()` is called.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ClientReadInitialMetadataReadableStreamSenderInitiation<Responder>{*this},
            detail::ClientReadInitialMetadataReadableStreamSenderImplementation{},
            static_cast<CompletionToken&&>(token));
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CLIENT_RPC_BASE_HPP
