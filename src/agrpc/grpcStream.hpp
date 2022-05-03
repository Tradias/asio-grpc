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

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include <agrpc/bindAllocator.hpp>
#include <agrpc/cancelSafe.hpp>
#include <agrpc/defaultCompletionToken.hpp>
#include <agrpc/detail/asyncInitiate.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpcContext.hpp>
#include <agrpc/grpcExecutor.hpp>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

template <class Executor>
class BasicGrpcStream
{
  private:
    template <class Allocator>
    class CompletionHandler;

  public:
    using executor_type = Executor;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicGrpcStream<OtherExecutor>;
    };

    template <class Exec>
    explicit BasicGrpcStream(Exec&& executor) noexcept : executor(std::forward<Exec>(executor))
    {
    }

    explicit BasicGrpcStream(agrpc::GrpcContext& grpc_context) noexcept : executor(grpc_context.get_executor()) {}

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto next(CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return safe.wait(std::forward<CompletionToken>(token));
    }

    template <class Allocator, class Function, class... Args>
    void initiate(std::allocator_arg_t, Allocator allocator, Function&& function, Args&&... args)
    {
        is_running = true;
        std::forward<Function>(function)(std::forward<Args>(args)..., CompletionHandler<Allocator>{*this, allocator});
    }

    template <class Function, class... Args>
    void initiate(Function&& function, Args&&... args)
    {
        this->initiate(std::allocator_arg, std::allocator<void>{}, std::forward<Function>(function),
                       std::forward<Args>(args)...);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto cleanup(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        if (!is_running)
        {
            return detail::async_initiate_immediate_completion<void(detail::ErrorCode, bool)>(token);
        }
        return safe.wait(std::forward<CompletionToken>(token));
    }

    executor_type get_executor() const noexcept { return executor; }

  private:
    template <class Allocator>
    class CompletionHandler
    {
      public:
        using executor_type = Executor;
        using allocator_type = Allocator;

        explicit CompletionHandler(BasicGrpcStream& stream, Allocator allocator) : impl(stream, allocator) {}

        void operator()(bool ok)
        {
            auto& self = impl.first();
            self.is_running = false;
            self.safe.token()(ok);
        }

        [[nodiscard]] executor_type get_executor() const noexcept { return impl.first().executor; }

        [[nodiscard]] allocator_type get_allocator() const noexcept { return impl.second(); }

      private:
        detail::CompressedPair<BasicGrpcStream&, Allocator> impl;
    };

    Executor executor;
    agrpc::GrpcCancelSafe safe;
    bool is_running{};
};

using GrpcStream = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicGrpcStream<agrpc::GrpcExecutor>>;

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_GRPCSTREAM_HPP
