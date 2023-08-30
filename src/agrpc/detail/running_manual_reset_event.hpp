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

#ifndef AGRPC_DETAIL_RUNNING_MANUAL_RESET_EVENT_HPP
#define AGRPC_DETAIL_RUNNING_MANUAL_RESET_EVENT_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/tuple.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/use_sender.hpp>
#include <grpcpp/server_context.h>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/cancel_safe.hpp>
#endif

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler, class... Args, std::size_t... I>
void invoke_with_args(CompletionHandler&& ch, const detail::Tuple<Args...>& args, const std::index_sequence<I...>&)
{
    static_cast<CompletionHandler&&>(ch)(detail::ErrorCode{}, detail::get<I>(args)...);
}

template <class CompletionHandler, class... Args>
void invoke_with_args(CompletionHandler&& ch, const detail::Tuple<Args...>& args)
{
    detail::invoke_with_args(static_cast<CompletionHandler&&>(ch), args, std::make_index_sequence<sizeof...(Args)>{});
}

template <class... Args>
class RunningManualResetEvent<void(Args...)> : public detail::OperationBase
{
  private:
    using Signature = void(Args...);

  public:
    RunningManualResetEvent() noexcept : detail::OperationBase{&do_complete} {}

    [[nodiscard]] void* tag() noexcept
    {
        running_.store(true, std::memory_order_relaxed);
        return static_cast<detail::OperationBase*>(this);
    }

    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_relaxed); }

    auto wait(const agrpc::GrpcContext&, agrpc::UseSender) { return event_.wait(); }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class CompletionToken>
    auto wait(agrpc::GrpcContext& grpc_context, CompletionToken token)
    {
        using Sig = detail::PrependErrorCodeToSignatureT<Signature>;
        return asio::async_initiate<CompletionToken, Sig>(
            [&](auto&& completion_handler)
            {
                using CompletionHandler = decltype(completion_handler);
                const auto allocator = asio::get_associated_allocator(completion_handler);
                if (event_.ready())
                {
                    auto executor = asio::get_associated_executor(completion_handler, grpc_context);
                    detail::post_with_allocator(
                        std::move(executor),
                        [&, ch = static_cast<CompletionHandler&&>(completion_handler)]() mutable
                        {
                            detail::invoke_with_args(static_cast<CompletionHandler&&>(ch), event_.args());
                        },
                        allocator);
                    return;
                }
                using Receiver = detail::CompletionHandlerReceiver<
                    detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>, Sig>;
                using Operation = ManualResetEventRunningOperationState<Signature, Receiver>;
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

  private:
    static void do_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* self = static_cast<RunningManualResetEvent*>(op);
        self->running_.store(false, std::memory_order_relaxed);
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            if constexpr ((false || ... || std::is_same_v<Args, bool>))
            {
                self->event_.set(detail::is_ok(result));
            }
            else
            {
                self->event_.set();
            }
        }
    }

    ManualResetEvent<Signature> event_;
    std::atomic<bool> running_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RUNNING_MANUAL_RESET_EVENT_HPP
