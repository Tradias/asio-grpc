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

#ifndef AGRPC_DETAIL_NOTIFY_WHEN_DONE_EVENT_HPP
#define AGRPC_DETAIL_NOTIFY_WHEN_DONE_EVENT_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_forward.hpp>
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
class NotifyWhenDoneEvent : public detail::OperationBase
{
  public:
    NotifyWhenDoneEvent() noexcept : detail::OperationBase{&do_complete} {}

    [[nodiscard]] void* tag() noexcept
    {
        running_.store(true, std::memory_order_relaxed);
        return static_cast<detail::OperationBase*>(this);
    }

    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_relaxed); }

    auto wait(const agrpc::GrpcContext&, agrpc::UseSender) noexcept { return event_.wait(); }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class CompletionToken>
    auto wait(agrpc::GrpcContext& grpc_context, CompletionToken&& token)
    {
        return event_.wait(static_cast<CompletionToken&&>(token), grpc_context.get_executor());
    }
#endif

  private:
    static void do_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto* self = static_cast<NotifyWhenDoneEvent*>(op);
        self->running_.store(false, std::memory_order_relaxed);
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            self->event_.set();
        }
    }

    ManualResetEvent<void()> event_;
    std::atomic_bool running_{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NOTIFY_WHEN_DONE_EVENT_HPP
