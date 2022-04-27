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

#ifndef AGRPC_AGRPC_GRPCSTREAM_HPP
#define AGRPC_AGRPC_GRPCSTREAM_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include "agrpc/bindAllocator.hpp"
#include "agrpc/cancelSafe.hpp"
#include "agrpc/defaultCompletionToken.hpp"
#include "agrpc/detail/asyncInitiate.hpp"
#include "agrpc/detail/utility.hpp"

#include <memory>

AGRPC_NAMESPACE_BEGIN()

class GrpcStream
{
  private:
    template <class Allocator>
    class CompletionToken;

  public:
    explicit GrpcStream(agrpc::GrpcContext& grpc_context) noexcept : grpc_context(grpc_context) {}

    template <class CompletionToken = agrpc::DefaultCompletionToken>
    auto next(CompletionToken&& token = {})
    {
        return safe.wait(std::forward<CompletionToken>(token));
    }

    template <class Allocator, class Function, class... Args>
    void initiate(std::allocator_arg_t, Allocator allocator, Function&& function, Args&&... args)
    {
        if (!is_done && !is_running)
        {
            is_running = true;
            std::forward<Function>(function)(std::forward<Args>(args)..., CompletionToken<Allocator>{*this, allocator});
        }
    }

    template <class Function, class... Args>
    void initiate(Function&& function, Args&&... args)
    {
        this->initiate(std::allocator_arg, std::allocator<void>{}, std::forward<Function>(function),
                       std::forward<Args>(args)...);
    }

    template <class CompletionToken = agrpc::DefaultCompletionToken>
    auto cleanup(CompletionToken token = {})
    {
        if (!is_running)
        {
            return detail::async_initiate_immediate_completion<void(detail::ErrorCode, bool)>(token);
        }
        return safe.wait(std::forward<CompletionToken>(token));
    }

  private:
    template <class Allocator>
    class CompletionToken
    {
      public:
        using executor_type = agrpc::GrpcContext::executor_type;
        using allocator_type = Allocator;

        explicit CompletionToken(GrpcStream& stream, Allocator allocator) : impl(stream, allocator) {}

        void operator()(bool ok)
        {
            auto& self = impl.first();
            self.is_done = !ok;
            self.is_running = false;
            self.safe.token()(ok);
        }

        [[nodiscard]] executor_type get_executor() const noexcept { return impl.first().grpc_context.get_executor(); }

        [[nodiscard]] allocator_type get_allocator() const noexcept { return impl.second(); }

      private:
        detail::CompressedPair<GrpcStream&, Allocator> impl;
    };

    agrpc::GrpcContext& grpc_context;
    agrpc::GrpcCancelSafe safe;
    bool is_running{};
    bool is_done{};
};

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_GRPCSTREAM_HPP
