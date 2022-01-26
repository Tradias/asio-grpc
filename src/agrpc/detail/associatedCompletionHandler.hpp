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

#ifndef AGRPC_DETAIL_ASSOCIATEDCOMPLETIONHANDLER_HPP
#define AGRPC_DETAIL_ASSOCIATEDCOMPLETIONHANDLER_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler>
struct AssociatedCompletionHandler
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    using executor_type = asio::associated_executor_t<CompletionHandler>;
    using allocator_type = asio::associated_allocator_t<CompletionHandler>;
#endif

    CompletionHandler completion_handler;

    explicit AssociatedCompletionHandler(CompletionHandler completion_handler)
        : completion_handler(std::move(completion_handler))
    {
    }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) &&
    {
        return std::move(this->completion_handler)(std::forward<Args>(args)...);
    }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) const&
    {
        return this->completion_handler(std::forward<Args>(args)...);
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    [[nodiscard]] executor_type get_executor() const noexcept
    {
        return asio::get_associated_executor(this->completion_handler);
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(this->completion_handler);
    }
#endif
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASSOCIATEDCOMPLETIONHANDLER_HPP
