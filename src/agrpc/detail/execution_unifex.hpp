// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_DETAIL_EXECUTION_UNIFEX_HPP
#define AGRPC_DETAIL_EXECUTION_UNIFEX_HPP

#include <agrpc/detail/utility.hpp>
#include <unifex/config.hpp>
#include <unifex/get_allocator.hpp>
#include <unifex/get_stop_token.hpp>
#include <unifex/inline_scheduler.hpp>
#include <unifex/receiver_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/stop_token_concepts.hpp>
#include <unifex/tag_invoke.hpp>
#include <unifex/then.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail::exec
{
using ::unifex::get_allocator;

using get_allocator_t = ::unifex::tag_t<::unifex::get_allocator>;

template <class Env>
using allocator_of_t = detail::RemoveCrefT<decltype(exec::get_allocator(std::declval<Env>()))>;

using ::unifex::get_scheduler;
using ::unifex::scheduler;
using ::unifex::scheduler_provider;

template <class T>
inline constexpr bool is_sender_v = ::unifex::sender<T>;

using ::unifex::connect;
using ::unifex::connect_result_t;
using ::unifex::get_stop_token;
using ::unifex::inline_scheduler;
using ::unifex::set_done;
using ::unifex::set_error;
using ::unifex::set_value;
using ::unifex::start;
using ::unifex::then;

using ::unifex::stop_token_type_t;

template <class Env>
using stop_token_of_t = typename Env::StopToken;

template <class T, class = void>
inline constexpr bool stoppable_token = false;

template <class T>
inline constexpr bool stoppable_token<T, decltype((void)std::declval<T>().stop_possible())> = true;

template <class T, class = std::false_type>
inline constexpr bool unstoppable_token = false;

template <class T>
using UnstoppableTokenHelper = std::bool_constant<(T{}.stop_possible())>;

template <class T>
inline constexpr bool unstoppable_token<T, exec::UnstoppableTokenHelper<T>> = true;

namespace env_ns
{
template <class StopTokenT, class AllocatorT>
struct Env
{
    using StopToken = StopTokenT;
    using Allocator = AllocatorT;

    friend StopTokenT tag_invoke(::unifex::tag_t<::unifex::get_stop_token>, const Env& env) noexcept
    {
        return env.stop_token_;
    }

    friend AllocatorT tag_invoke(get_allocator_t, const Env& env) noexcept { return env.allocator_; }

    StopTokenT stop_token_;
    AllocatorT allocator_;
};

template <class StopTokenT, class AllocatorT>
Env(StopTokenT, AllocatorT) -> Env<StopTokenT, AllocatorT>;
}

using env_ns::Env;

template <class Receiver>
auto get_env(const Receiver& receiver) noexcept
{
    return Env{::unifex::get_stop_token(receiver), ::unifex::get_allocator(receiver)};
}

template <class Receiver>
using env_of_t = decltype(exec::get_env(std::declval<Receiver>()));

using ::unifex::tag_t;
}  // namespace exec

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_EXECUTION_UNIFEX_HPP
