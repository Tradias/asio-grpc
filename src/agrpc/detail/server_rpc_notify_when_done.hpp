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

#include "agrpc/detail/completion_handler_receiver.hpp"
#include "agrpc/detail/work_tracking_completion_handler.hpp"

#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/grpc_submit.hpp>
#include <agrpc/detail/intrusive_list_hook.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/operation.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/use_sender.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct NotifyWhenDoneInitFunction
{
    grpc::ServerContext& server_context_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { server_context_.AsyncNotifyWhenDone(tag); }
};

class NotifyWhenDone
{
  private:
    template <class T>
    struct Allocator;

    struct CompletionHandler;

    using Operation = detail::GrpcTagOperation<CompletionHandler>;

    struct CompletionHandler
    {
        using allocator_type = Allocator<Operation>;

        void operator()(bool) const
        {
            self_.running_ = false;
            self_.event_.set();
        }

        allocator_type get_allocator() const noexcept;

        NotifyWhenDone& self_;
    };

    static_assert(std::is_trivially_destructible_v<Operation>);

    template <class Handler>
    void async_notify_when_done(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context,
                                Handler&& handler)
    {
        detail::NotifyWhenDoneInitFunction init{server_context};
        detail::grpc_submit(grpc_context, init, std::forward<Handler>(handler));
    }

  public:
    NotifyWhenDone() noexcept {}

    void initiate(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context)
    {
        async_notify_when_done(grpc_context, server_context, CompletionHandler{*this});
    }

    [[nodiscard]] bool is_running() const noexcept { return running_; }

    template <class CompletionToken>
    auto done(CompletionToken token);

    auto done(agrpc::UseSender) { return event_.wait(); }

  private:
    ManualResetEvent event_;
    bool running_{true};
    union
    {
        Operation operation_;
    };
};

template <class T>
struct NotifyWhenDone::Allocator
{
    using value_type = T;

    Allocator() = default;

    constexpr explicit Allocator(Operation* operation) noexcept : operation_(operation) {}

    template <class U>
    constexpr Allocator(const Allocator<U>& other) noexcept : operation_(other.operation_)
    {
    }

    [[nodiscard]] constexpr T* allocate(std::size_t) noexcept { return operation_; }

    static constexpr void deallocate(T*, std::size_t) noexcept {}

    template <class U>
    friend constexpr bool operator==(const Allocator&, const Allocator<U>&) noexcept
    {
        return true;
    }

    template <class U>
    friend constexpr bool operator!=(const Allocator&, const Allocator<U>&) noexcept
    {
        return false;
    }

    template <class>
    friend struct Allocator;

    Operation* operation_;
};

inline NotifyWhenDone::CompletionHandler::allocator_type NotifyWhenDone::CompletionHandler::get_allocator()
    const noexcept
{
    return Allocator<Operation>{&self_.operation_};
}

template <class CompletionToken>
inline auto NotifyWhenDone::done(CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void()>(
        [&](auto&& completion_handler)
        {
            using CompletionHandler = decltype(completion_handler);
            using Receiver = detail::CompletionHandlerReceiver<
                detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>>;
            using ManualResetEventOperation =
                decltype(std::declval<ManualResetEvent>().wait().connect(std::declval<Receiver>()));
            const auto allocator = detail::exec::get_allocator(completion_handler);
            Receiver handler{static_cast<CompletionHandler&&>(completion_handler)};
            auto operation =
                detail::allocate<ManualResetEventOperation>(allocator, static_cast<Receiver&&>(handler), event_);
            operation->start();
            operation.release();
        },
        token);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_HPP
