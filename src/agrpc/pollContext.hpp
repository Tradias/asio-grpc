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

#ifndef AGRPC_AGRPC_POLLCONTEXT_HPP
#define AGRPC_AGRPC_POLLCONTEXT_HPP

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/oneShotAllocator.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Helper class to repeatedly poll a GrpcContext in a different execution context
 *
 * @since 1.5.0
 */
template <class Executor>
class PollContext
{
  public:
    explicit PollContext(Executor executor)
        : executor(asio::prefer(asio::require(executor, asio::execution::blocking.never),
                                asio::execution::relationship.continuation,
                                asio::execution::allocator(detail::OneShotAllocator<std::byte, 80>{&buffer})))
    {
    }

    void poll(agrpc::GrpcContext& grpc_context)
    {
        if (grpc_context.is_stopped())
        {
            return;
        }
        asio::execution::execute(std::as_const(executor),
                                 [&]
                                 {
                                     grpc_context.poll();
                                     poll(grpc_context);
                                 });
    }

  private:
    using Exec = decltype(asio::prefer(asio::require(std::declval<Executor>(), asio::execution::blocking_t::never),
                                       asio::execution::relationship_t::continuation,
                                       asio::execution::allocator(detail::OneShotAllocator<std::byte, 80>{nullptr})));

    Exec executor;
    std::aligned_storage_t<80> buffer;
};

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_POLLCONTEXT_HPP
