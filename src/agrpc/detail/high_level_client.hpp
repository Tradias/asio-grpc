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

#ifndef AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP
#define AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/async_initiate.hpp>
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/conditional_sender.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/tagged_ptr.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/use_sender.hpp>
#include <grpcpp/client_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class AutoCancelClientContextRef
{
  public:
    AutoCancelClientContextRef() = default;

    explicit AutoCancelClientContextRef(grpc::ClientContext& context) noexcept : context(context) {}

    AutoCancelClientContextRef(const AutoCancelClientContextRef&) = delete;

    AutoCancelClientContextRef(AutoCancelClientContextRef&& other) noexcept : context(other.context)
    {
        other.context.clear();
    }

    ~AutoCancelClientContextRef() noexcept
    {
        if (const auto context_ptr = context.get())
        {
            context_ptr->TryCancel();
        }
    }

    AutoCancelClientContextRef& operator=(const AutoCancelClientContextRef&) = delete;

    AutoCancelClientContextRef& operator=(AutoCancelClientContextRef&& other) noexcept
    {
        if (this != &other)
        {
            if (const auto context_ptr = context.get())
            {
                context_ptr->TryCancel();
            }
            context = other.context;
            other.context.clear();
        }
        return *this;
    }

    void clear() noexcept { context.clear(); }

    [[nodiscard]] bool is_null() const noexcept { return context.is_null(); }

    template <std::uintptr_t Bit>
    [[nodiscard]] bool has_bit() const noexcept
    {
        return context.template has_bit<Bit>();
    }

    template <std::uintptr_t Bit>
    void set_bit() noexcept
    {
        context.template set_bit<Bit>();
    }

  private:
    detail::TaggedPtr<grpc::ClientContext> context{};
};

class RPCClientContextBase
{
  protected:
    RPCClientContextBase() = default;

    explicit RPCClientContextBase(grpc::ClientContext& client_context) : client_context(client_context) {}

    [[nodiscard]] bool is_finished() const noexcept { return client_context.is_null(); }

    void set_finished() noexcept { client_context.clear(); }

    [[nodiscard]] bool is_writes_done() const noexcept { return client_context.has_bit<0>(); }

    void set_writes_done() noexcept { client_context.set_bit<0>(); }

  private:
    detail::AutoCancelClientContextRef client_context;
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct SubmitSenderToWorkTrackingCompletionHandler
{
    template <class CompletionHandler, class Implementation>
    void operator()(CompletionHandler&& completion_handler,
                    const typename detail::RemoveCrefT<Implementation>::Initiation& initiation,
                    Implementation&& implementation)
    {
        detail::submit_basic_sender_running_operation(
            grpc_context,
            detail::CompletionHandlerReceiver<
                detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CompletionHandler>>>(
                static_cast<CompletionHandler&&>(completion_handler)),
            initiation, static_cast<Implementation&&>(implementation));
    }

    agrpc::GrpcContext& grpc_context;
};
#endif

template <class Implementation, class CompletionToken>
auto async_initiate_sender_implementation(agrpc::GrpcContext& grpc_context,
                                          const typename Implementation::Initiation& initiation,
                                          Implementation&& implementation, [[maybe_unused]] CompletionToken& token)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, CompletionToken>)
    {
        return asio::async_initiate<CompletionToken, typename Implementation::Signature>(
            detail::SubmitSenderToWorkTrackingCompletionHandler{grpc_context}, token, initiation,
            static_cast<Implementation&&>(implementation));
    }
    else
#endif
    {
        return detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation,
                                                                 static_cast<Implementation&&>(implementation));
    }
}

template <class Implementation, class CompletionToken, class... Args>
auto async_initiate_conditional_sender_implementation(agrpc::GrpcContext& grpc_context,
                                                      const typename Implementation::Initiation& initiation,
                                                      Implementation&& implementation, bool condition,
                                                      [[maybe_unused]] CompletionToken& token, Args&&... args)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, CompletionToken>)
    {
        using Signature = typename Implementation::Signature;
        if (condition)
        {
            return asio::async_initiate<CompletionToken, Signature>(
                detail::SubmitSenderToWorkTrackingCompletionHandler{grpc_context}, token, initiation,
                static_cast<Implementation&&>(implementation));
        }
        else
        {
            return detail::async_initiate_immediate_completion<Signature>(static_cast<CompletionToken&&>(token),
                                                                          static_cast<Args&&>(args)...);
        }
    }
    else
#endif
    {
        return detail::ConditionalSenderAccess::create(
            detail::BasicSenderAccess::create<Implementation>(grpc_context, initiation,
                                                              static_cast<Implementation&&>(implementation)),
            condition, static_cast<Args&&>(args)...);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_HPP
