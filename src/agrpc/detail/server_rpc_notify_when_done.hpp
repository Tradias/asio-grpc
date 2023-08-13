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

#ifndef AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_HPP
#define AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/use_sender.hpp>
#include <grpcpp/server_context.h>

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class NotifyWhenDone : public detail::OperationBase
{
  public:
    NotifyWhenDone() noexcept : detail::OperationBase{&do_complete} {}

    void initiate(grpc::ServerContext& server_context)
    {
        server_context.AsyncNotifyWhenDone(static_cast<detail::OperationBase*>(this));
    }

    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_relaxed); }

    template <class CompletionToken>
    auto done(agrpc::GrpcContext& grpc_context, CompletionToken token);

    auto done(const agrpc::GrpcContext&, agrpc::UseSender) { return event_.wait(); }

  private:
    static void do_complete(detail::OperationBase* op, detail::OperationResult, agrpc::GrpcContext&)
    {
        auto* self = static_cast<NotifyWhenDone*>(op);
        self->running_.store(true, std::memory_order_relaxed);
        self->event_.set();
    }

    ManualResetEvent event_;
    std::atomic<bool> running_{true};
};

#ifndef AGRPC_UNIFEX
template <class CompletionToken>
inline auto NotifyWhenDone::done(agrpc::GrpcContext& grpc_context, CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void()>(
        [&](auto&& completion_handler)
        {
            using CompletionHandler = decltype(completion_handler);
            const auto allocator = asio::get_associated_allocator(completion_handler);
            if (event_.ready())
            {
                auto executor = asio::get_associated_executor(completion_handler, grpc_context);
                detail::post_with_allocator(std::move(executor), static_cast<CompletionHandler&&>(completion_handler),
                                            allocator);
                return;
            }
            using Receiver = detail::CompletionHandlerReceiver<
                detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>>;
            using Operation = ManualResetEventRunningOperationState<Receiver>;
            Receiver receiver{static_cast<CompletionHandler&&>(completion_handler)};
            if (detail::check_start_conditions(receiver))
            {
                auto operation = detail::allocate<Operation>(allocator, static_cast<Receiver&&>(receiver), event_,
                                                             DeallocateOnCompleteArg<DeallocateOnComplete::YES>{});
                operation->start();
                operation.release();
            }
        },
        token);
}
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_HPP
