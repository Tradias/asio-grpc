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

#ifndef AGRPC_DETAIL_EXECUTION_STDEXEC_HPP
#define AGRPC_DETAIL_EXECUTION_STDEXEC_HPP

#include <exec/inline_scheduler.hpp>
#include <stdexec/execution.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail::exec
{
using ::stdexec::get_allocator_t;

struct GetAllocatorFn
{
    template <class Receiver>
    decltype(auto) operator()(const Receiver& receiver) const
    {
        if constexpr (::stdexec::tag_invocable<::stdexec::get_allocator_t, ::stdexec::env_of_t<Receiver>>)
        {
            return ::stdexec::get_allocator(::stdexec::get_env(receiver));
        }
        else
        {
            return std::allocator<std::byte>{};
        }
    }
};

inline constexpr GetAllocatorFn get_allocator{};

using ::stdexec::get_scheduler;

using ::stdexec::scheduler;

template <class, class = void>
inline constexpr bool scheduler_provider = false;

template <class T>
inline constexpr bool scheduler_provider<T, decltype((void)exec::get_scheduler(std::declval<const T&>()))> = true;

template <class T>
inline constexpr bool is_sender_v = ::stdexec::sender<T>;

using ::exec::inline_scheduler;
using ::stdexec::connect;
using ::stdexec::connect_result_t;
using ::stdexec::then;

template <class Receiver>
decltype(auto) get_stop_token(const Receiver& receiver)
{
    return ::stdexec::get_stop_token(::stdexec::get_env(receiver));
}

template <class Receiver>
void set_done(Receiver&& receiver) noexcept
{
    ::stdexec::set_stopped(static_cast<Receiver&&>(receiver));
}

using ::stdexec::set_error;
using ::stdexec::set_value;
using ::stdexec::start;

template <class Receiver>
using stop_token_type_t = ::stdexec::stop_token_of_t<::stdexec::env_of_t<Receiver>>;

using ::stdexec::stoppable_token;
using ::stdexec::unstoppable_token;

using ::stdexec::tag_t;
}  // namespace exec

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_EXECUTION_STDEXEC_HPP
