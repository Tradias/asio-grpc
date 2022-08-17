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

#ifndef AGRPC_DETAIL_ASSOCIATED_COMPLETION_HANDLER_HPP
#define AGRPC_DETAIL_ASSOCIATED_COMPLETION_HANDLER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/memory_resource.hpp>

#include <memory>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler>
class AssociatedCompletionHandler
{
  public:
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    using executor_type = asio::associated_executor_t<CompletionHandler>;
    using allocator_type = asio::associated_allocator_t<CompletionHandler>;
#endif

    template <class... Args>
    explicit AssociatedCompletionHandler(Args&&... args) : completion_handler_(static_cast<Args&&>(args)...)
    {
    }

    [[nodiscard]] auto& completion_handler() noexcept { return completion_handler_; }

    [[nodiscard]] auto& completion_handler() const noexcept { return completion_handler_; }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) &&
    {
        return std::move(this->completion_handler_)(static_cast<Args&&>(args)...);
    }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) const&
    {
        return this->completion_handler_(static_cast<Args&&>(args)...);
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    [[nodiscard]] executor_type get_executor() const noexcept
    {
        return asio::get_associated_executor(this->completion_handler_);
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(this->completion_handler_);
    }
#endif

  private:
    CompletionHandler completion_handler_;
};
}

AGRPC_NAMESPACE_END

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <template <class, class> class Associator, class CompletionHandler, class DefaultCandidate>
struct agrpc::asio::associator<Associator, agrpc::detail::AssociatedCompletionHandler<CompletionHandler>,
                               DefaultCandidate>
{
    using type = typename Associator<CompletionHandler, DefaultCandidate>::type;

    static type get(const agrpc::detail::AssociatedCompletionHandler<CompletionHandler>& b,
                    const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<CompletionHandler, DefaultCandidate>::get(b.completion_handler(), c);
    }
};

#endif

template <class CompletionHandler, class Alloc>
struct agrpc::detail::container::uses_allocator<agrpc::detail::AssociatedCompletionHandler<CompletionHandler>, Alloc>
    : std::false_type
{
};

#endif  // AGRPC_DETAIL_ASSOCIATED_COMPLETION_HANDLER_HPP
