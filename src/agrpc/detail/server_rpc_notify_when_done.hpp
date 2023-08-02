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

#include <agrpc/cancel_safe.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/grpc_submit.hpp>
#include <agrpc/detail/intrusive_list_hook.hpp>
#include <agrpc/detail/operation.hpp>
#include <agrpc/detail/sender_implementation.hpp>
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
            self_.safe_.token()();
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
    auto done(CompletionToken&& token)
    {
        return safe_.wait(static_cast<CompletionToken&&>(token));
    }

  private:
    agrpc::CancelSafe<void()> safe_;
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
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_NOTIFY_WHEN_DONE_HPP
