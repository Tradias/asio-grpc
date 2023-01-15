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

#ifndef AGRPC_DETAIL_WORK_TRACKING_COMPLETION_HANDLER_HPP
#define AGRPC_DETAIL_WORK_TRACKING_COMPLETION_HANDLER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

#include <agrpc/detail/asio_association.hpp>
#include <agrpc/detail/memory_resource.hpp>
#include <agrpc/detail/tuple.hpp>
#include <agrpc/detail/utility.hpp>

#include <memory>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Executor>
inline constexpr bool IS_INLINE_EXECUTOR = false;

template <class Relationship, class Allocator>
inline constexpr bool
    IS_INLINE_EXECUTOR<asio::basic_system_executor<asio::execution::blocking_t::possibly_t, Relationship, Allocator>> =
        true;

template <class Relationship, class Allocator>
inline constexpr bool
    IS_INLINE_EXECUTOR<asio::basic_system_executor<asio::execution::blocking_t::always_t, Relationship, Allocator>> =
        true;

template <class CompletionHandler, bool = detail::IS_INLINE_EXECUTOR<asio::associated_executor_t<CompletionHandler>>>
class WorkTracker
{
  public:
    explicit WorkTracker(const CompletionHandler& ch)
        : work_(asio::prefer(asio::get_associated_executor(ch), asio::execution::outstanding_work_t::tracked))
    {
    }

  private:
    typename asio::prefer_result<asio::associated_executor_t<CompletionHandler>,
                                 asio::execution::outstanding_work_t::tracked_t>::type work_;
};

template <class CompletionHandler>
class WorkTracker<CompletionHandler, true>
{
  public:
    constexpr explicit WorkTracker(const CompletionHandler&) noexcept {}
};

template <class CompletionHandler>
class WorkTrackingCompletionHandler : private detail::EmptyBaseOptimization<CompletionHandler>,
                                      private detail::WorkTracker<CompletionHandler>

{
  public:
    using executor_type = asio::associated_executor_t<CompletionHandler>;
    using allocator_type = asio::associated_allocator_t<CompletionHandler>;

  private:
    using CompletionHandlerBase = detail::EmptyBaseOptimization<CompletionHandler>;
    using WorkTrackerBase = detail::WorkTracker<CompletionHandler>;

  public:
    template <class Ch>
    explicit WorkTrackingCompletionHandler(Ch&& ch)
        : CompletionHandlerBase(static_cast<Ch&&>(ch)), WorkTrackerBase(completion_handler())
    {
    }

    [[nodiscard]] auto& completion_handler() noexcept { return this->get(); }

    [[nodiscard]] auto& completion_handler() const noexcept { return this->get(); }

    template <class... Args>
    void operator()(Args&&... args) &&
    {
        auto& ch = completion_handler();
        auto executor = asio::prefer(asio::get_associated_executor(ch), asio::execution::blocking_t::possibly,
                                     asio::execution::allocator(asio::get_associated_allocator(ch)));
        detail::do_execute(
            std::move(executor),
            [ch = static_cast<CompletionHandler&&>(ch), args = detail::Tuple{static_cast<Args&&>(args)...}]() mutable
            {
                detail::apply(static_cast<CompletionHandler&&>(ch), std::move(args));
            });
    }

    [[nodiscard]] decltype(auto) get_executor() const noexcept
    {
        return asio::get_associated_executor(completion_handler());
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(completion_handler());
    }
};
}

AGRPC_NAMESPACE_END

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <template <class, class> class Associator, class CompletionHandler, class DefaultCandidate>
struct agrpc::asio::associator<Associator, agrpc::detail::WorkTrackingCompletionHandler<CompletionHandler>,
                               DefaultCandidate>
{
    using type = typename Associator<CompletionHandler, DefaultCandidate>::type;

    static decltype(auto) get(const agrpc::detail::WorkTrackingCompletionHandler<CompletionHandler>& b,
                              const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<CompletionHandler, DefaultCandidate>::get(b.completion_handler(), c);
    }
};

#endif

template <class CompletionHandler, class Alloc>
struct agrpc::detail::container::uses_allocator<agrpc::detail::WorkTrackingCompletionHandler<CompletionHandler>, Alloc>
    : std::false_type
{
};

#endif

#endif  // AGRPC_DETAIL_WORK_TRACKING_COMPLETION_HANDLER_HPP
