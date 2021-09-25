// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_COMPLETIONHANDLERWITHPAYLOAD_HPP
#define AGRPC_DETAIL_COMPLETIONHANDLERWITHPAYLOAD_HPP

#include "agrpc/detail/asioForward.hpp"

#include <utility>

namespace agrpc::detail
{
template <class Payload, class CompletionHandler>
struct CompletionHandlerWithPayload
{
    using executor_type = asio::associated_executor_t<CompletionHandler>;
    using allocator_type = asio::associated_allocator_t<CompletionHandler>;

    CompletionHandler completion_handler;
    Payload payload;

    template <class... Args>
    CompletionHandlerWithPayload(CompletionHandler completion_handler, Args&&... args)
        : completion_handler(std::move(completion_handler)), payload(std::forward<Args>(args)...)
    {
    }

    decltype(auto) operator()(bool ok) &&
    {
        return std::move(this->completion_handler)(std::pair{std::move(this->payload), ok});
    }

    [[nodiscard]] executor_type get_executor() const noexcept
    {
        return asio::get_associated_executor(this->completion_handler);
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(this->completion_handler);
    }
};

template <class Payload, class CompletionHandler, class... Args>
auto make_completion_handler_with_payload(CompletionHandler completion_handler, Args&&... args)
{
    return detail::CompletionHandlerWithPayload<Payload, CompletionHandler>{std::move(completion_handler),
                                                                            std::forward<Args>(args)...};
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_COMPLETIONHANDLERWITHPAYLOAD_HPP
