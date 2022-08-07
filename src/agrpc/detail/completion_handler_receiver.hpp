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

#ifndef AGRPC_DETAIL_COMPLETION_HANDLER_RECEIVER_HPP
#define AGRPC_DETAIL_COMPLETION_HANDLER_RECEIVER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler>
class CompletionHandlerReceiver
{
  public:
    using allocator_type = detail::AssociatedAllocatorT<CompletionHandler>;

    template <class Ch>
    explicit CompletionHandlerReceiver(Ch&& ch) : completion_handler_(std::forward<Ch>(ch))
    {
    }

    static void set_done() noexcept {}

    template <class... Args>
    void set_value(Args&&... args)
    {
        std::move(completion_handler_)(std::forward<Args>(args)...);
    }

    static void set_error(std::exception_ptr ep) { std::rethrow_exception(ep); }

    allocator_type get_allocator() const noexcept { return detail::exec::get_allocator(completion_handler_); }

    const CompletionHandler& completion_handler() const noexcept { return completion_handler_; }

  private:
    CompletionHandler completion_handler_;
};
}

AGRPC_NAMESPACE_END

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <template <class, class> class Associator, class CompletionHandler, class DefaultCandidate>
struct agrpc::asio::associator<Associator, agrpc::detail::CompletionHandlerReceiver<CompletionHandler>,
                               DefaultCandidate>
{
    using type = typename Associator<CompletionHandler, DefaultCandidate>::type;

    static constexpr type get(const agrpc::detail::CompletionHandlerReceiver<CompletionHandler>& b,
                              const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<CompletionHandler, DefaultCandidate>::get(b.completion_handler(), c);
    }
};

#endif

#endif  // AGRPC_DETAIL_COMPLETION_HANDLER_RECEIVER_HPP
