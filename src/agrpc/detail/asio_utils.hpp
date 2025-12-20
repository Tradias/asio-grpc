// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_ASIO_UTILS_HPP
#define AGRPC_DETAIL_ASIO_UTILS_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/asio_macros.hpp>
#include <agrpc/detail/config.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class, class = void>
inline constexpr bool IS_EXECUTOR_PROVIDER = false;

template <class T>
inline constexpr bool IS_EXECUTOR_PROVIDER<T, decltype((void)std::declval<T>().get_executor())> = true;

template <class Handler, class Function>
struct AllocatorAssociator
{
    using allocator_type = asio::associated_allocator_t<Handler>;

    void operator()() { static_cast<Function&&>(function_)(static_cast<Handler&&>(handler_)); }

    allocator_type get_allocator() const noexcept { return assoc::get_associated_allocator(handler_); }

    Handler handler_;
    Function function_;
};

template <class Handler, class Function>
AllocatorAssociator(const Handler&, const Function&) -> AllocatorAssociator<Handler, Function>;

template <class CompletionHandler, class Function, class IOExecutor>
void complete_immediately(CompletionHandler&& completion_handler, Function&& function, const IOExecutor& io_executor)
{
#ifdef AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR
    auto executor = asio::get_associated_immediate_executor(
        completion_handler,
        [&]() -> decltype(auto)
        {
            // Ensure that the io_executor is not already blocking::never because asio will try to convert const& to &&
            // due to chosing the `identity` overload of asio::require within the default_immediate_executor.
            // https://github.com/chriskohlhoff/asio/issues/1392
            if constexpr (asio::traits::static_require<IOExecutor, asio::execution::blocking_t::never_t>::is_valid)
            {
                return asio::prefer(io_executor, asio::execution::blocking_t::possibly);
            }
            else
            {
                return (io_executor);
            }
        }());
    asio::dispatch(std::move(executor),
                   AllocatorAssociator{static_cast<CompletionHandler&&>(completion_handler),
                                       [f = static_cast<Function&&>(function)](auto&& ch) mutable
                                       {
                                           static_cast<Function&&>(f)(static_cast<decltype(ch)&&>(ch));
                                       }});
#else
    auto executor = assoc::get_associated_executor(completion_handler, io_executor);
    asio::post(std::move(executor), AllocatorAssociator{static_cast<CompletionHandler&&>(completion_handler),
                                                        [f = static_cast<Function&&>(function)](auto&& ch) mutable
                                                        {
                                                            static_cast<Function&&>(f)(static_cast<decltype(ch)&&>(ch));
                                                        }});
#endif
}

template <class Object>
auto get_cancellation_slot([[maybe_unused]] const Object& object) noexcept
{
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    return asio::get_associated_cancellation_slot(object, UncancellableSlot{});
#else
    return UncancellableSlot{};
#endif
}

template <class T>
using CancellationSlotT = decltype(detail::get_cancellation_slot(std::declval<T>()));
}  // namespace detail

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_ASIO_UTILS_HPP
