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

#ifndef AGRPC_DETAIL_WORKTRACKINGCOMPLETIONHANDLER_HPP
#define AGRPC_DETAIL_WORKTRACKINGCOMPLETIONHANDLER_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/utility.hpp"

#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
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

template <class T>
using AssociatedWorkTrackingExecutor =
    std::conditional_t<detail::IS_INLINE_EXECUTOR<asio::associated_executor_t<T>>, detail::Empty,
                       std::decay_t<typename asio::prefer_result<
                           asio::associated_executor_t<T>, asio::execution::outstanding_work_t::tracked_t>::type>>;

template <class CompletionHandler>
class WorkTrackingCompletionHandler
    : private detail::EmptyBaseOptimization<detail::AssociatedWorkTrackingExecutor<CompletionHandler>>,
      private detail::EmptyBaseOptimization<CompletionHandler>
{
#ifndef AGRPC_ASIO_HAS_CANCELLATION_SLOT
  public:
    using executor_type = asio::associated_executor_t<CompletionHandler>;
    using allocator_type = asio::associated_allocator_t<CompletionHandler>;
#endif

  private:
    using WorkTracker = detail::AssociatedWorkTrackingExecutor<CompletionHandler>;
    using Base1 = detail::EmptyBaseOptimization<WorkTracker>;
    using Base2 = detail::EmptyBaseOptimization<CompletionHandler>;

  public:
    template <class Ch>
    explicit WorkTrackingCompletionHandler(Ch&& completion_handler)
        : Base1(detail::InplaceWithFunction{},
                [&]
                {
                    return WorkTrackingCompletionHandler::create_work_tracker(completion_handler);
                }),
          Base2(std::forward<Ch>(completion_handler))
    {
    }

    [[nodiscard]] auto& completion_handler() noexcept { return static_cast<Base2*>(this)->get(); }

    [[nodiscard]] auto& completion_handler() const noexcept { return static_cast<const Base2*>(this)->get(); }

    template <class... Args>
    void operator()(Args&&... args) &&
    {
        WorkTrackingCompletionHandler::complete(std::move(static_cast<Base1*>(this)->get()),
                                                std::move(this->completion_handler()), std::forward<Args>(args)...);
    }

#ifndef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    [[nodiscard]] executor_type get_executor() const noexcept
    {
        return asio::get_associated_executor(this->completion_handler());
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(this->completion_handler());
    }
#endif

  private:
    static constexpr decltype(auto) create_work_tracker(CompletionHandler& ch) noexcept
    {
        if constexpr (std::is_same_v<detail::Empty, WorkTracker>)
        {
            return detail::Empty{};
        }
        else
        {
            return asio::prefer(asio::get_associated_executor(ch), asio::execution::outstanding_work_t::tracked);
        }
    }

    template <class... Args>
    static constexpr void complete(WorkTracker, CompletionHandler&& ch, Args&&... args) noexcept
    {
        auto executor = asio::prefer(asio::get_associated_executor(ch), asio::execution::blocking_t::possibly,
                                     asio::execution::allocator(asio::get_associated_allocator(ch)));
        asio::execution::execute(std::move(executor),
                                 [ch = std::move(ch), args = std::make_tuple(std::forward<Args>(args)...)]() mutable
                                 {
                                     std::apply(std::move(ch), std::move(args));
                                 });
    }
};
#endif
}

AGRPC_NAMESPACE_END

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
AGRPC_ASIO_NAMESPACE_BEGIN()

template <template <class, class> class Associator, class CompletionHandler, class DefaultCandidate>
struct associator<Associator, ::agrpc::detail::WorkTrackingCompletionHandler<CompletionHandler>, DefaultCandidate>
{
    using type = typename Associator<CompletionHandler, DefaultCandidate>::type;

    static type get(const ::agrpc::detail::WorkTrackingCompletionHandler<CompletionHandler>& b,
                    const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<CompletionHandler, DefaultCandidate>::get(b.completion_handler(), c);
    }
};

AGRPC_ASIO_NAMESPACE_END
#endif

#endif  // AGRPC_DETAIL_WORKTRACKINGCOMPLETIONHANDLER_HPP
