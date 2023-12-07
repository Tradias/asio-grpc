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

#ifndef AGRPC_DETAIL_INITIATE_SENDER_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_INITIATE_SENDER_IMPLEMENTATION_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/use_sender.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/sender_implementation_operation.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct SubmitSenderImplementationOperation
{
    using executor_type = agrpc::GrpcExecutor;

    template <class CompletionHandler, class Initiation, class Implementation>
    void operator()(CompletionHandler&& completion_handler, const Initiation& initiation,
                    Implementation&& implementation)
    {
        detail::submit_sender_implementation_operation(grpc_context_,
                                                       static_cast<CompletionHandler&&>(completion_handler), initiation,
                                                       static_cast<Implementation&&>(implementation));
    }

    executor_type get_executor() const noexcept { return grpc_context_.get_executor(); }

    agrpc::GrpcContext& grpc_context_;
};
#endif

template <class Initiation, class Implementation, class CompletionToken>
auto async_initiate_sender_implementation(agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                                          Implementation&& implementation, [[maybe_unused]] CompletionToken&& token)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (!std::is_same_v<agrpc::UseSender, detail::RemoveCrefT<CompletionToken>>)
    {
        return asio::async_initiate<CompletionToken, typename Implementation::Signature>(
            detail::SubmitSenderImplementationOperation{grpc_context}, token, initiation,
            static_cast<Implementation&&>(implementation));
    }
    else
#endif
    {
        return detail::BasicSenderAccess::create<Initiation, Implementation>(
            grpc_context, initiation, static_cast<Implementation&&>(implementation));
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INITIATE_SENDER_IMPLEMENTATION_HPP
