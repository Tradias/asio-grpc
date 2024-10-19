// Copyright 2024 Dennis Hezel
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
#include <agrpc/detail/asio_utils.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/tuple.hpp>
#include <agrpc/detail/utility.hpp>

#include <utility>

#include <agrpc/detail/config.hpp>

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

template <class Executor, bool = detail::IS_INLINE_EXECUTOR<Executor>>
class WorkTracker
{
#if defined(ASIO_USE_TS_EXECUTOR_AS_DEFAULT) || defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)
  public:
    explicit WorkTracker(Executor&& executor) : work_(std::move(executor)) {}

  private:
    asio::executor_work_guard<Executor> work_;
#else
  public:
    explicit WorkTracker(Executor&& executor)
        : work_(asio::prefer(std::move(executor), asio::execution::outstanding_work_t::tracked))
    {
    }

  private:
    typename asio::prefer_result<Executor, asio::execution::outstanding_work_t::tracked_t>::type work_;
#endif
};

template <class Executor>
class WorkTracker<Executor, true>
{
  public:
    constexpr explicit WorkTracker(const Executor&) noexcept {}
};

template <class Handler, class... Args>
void dispatch_with_args(Handler&& handler, Args&&... args)
{
    auto executor = asio::get_associated_executor(handler);
    asio::dispatch(std::move(executor),
                   AllocatorAssociator{static_cast<Handler&&>(handler),
                                       [arg_tuple = detail::Tuple{static_cast<Args&&>(args)...}](auto&& ch) mutable
                                       {
                                           detail::apply(static_cast<decltype(ch)&&>(ch), std::move(arg_tuple));
                                       }});
}

template <class AllocationGuard, class... Args>
void dispatch_complete(AllocationGuard& guard, Args&&... args)
{
    auto& operation = *guard;
    auto handler{std::move(operation.completion_handler())};
    [[maybe_unused]] auto tracker{std::move(operation.work_tracker())};
    guard.reset();
    detail::dispatch_with_args(std::move(handler), static_cast<Args&&>(args)...);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_WORK_TRACKING_COMPLETION_HANDLER_HPP
