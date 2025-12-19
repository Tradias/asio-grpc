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

#ifndef AGRPC_UTILS_EXECUTION_UTILS_HPP
#define AGRPC_UTILS_EXECUTION_UTILS_HPP

#include "utils/asio_forward.hpp"

namespace test
{
#ifdef AGRPC_UNIFEX
inline constexpr auto& let_stopped = ::unifex::let_done;
inline constexpr auto& unstoppable = ::unifex::unstoppable;
inline constexpr auto& with_query_value = ::unifex::with_query_value;

template <class Sender>
decltype(auto) with_inline_scheduler(Sender&& sender)
{
    return unifex::with_query_value(std::forward<Sender>(sender), stdexec::get_scheduler, exec::inline_scheduler{});
}

using ::unifex::sync_wait;

template <class Scope, class Sender, class Scheduler = exec::inline_scheduler>
void scope_spawn_detached(Scope& scope, Sender&& sender, Scheduler&& scheduler = {})
{
    scope.detached_spawn(stdexec::on(std::forward<Scheduler>(scheduler), std::forward<Sender>(sender)));
}

template <class Scope>
decltype(auto) scope_on_empty(Scope& scope)
{
    return scope.complete();
}
#else
using ::stdexec::let_stopped;

template <class Sender>
decltype(auto) unstoppable(Sender&& sender)
{
    return std::forward<Sender>(sender);
}

template <class Sender, class... T>
decltype(auto) with_query_value(Sender&& sender, T&&...)
{
    return std::forward<Sender>(sender);
}

template <class Sender>
decltype(auto) with_inline_scheduler(Sender&& sender)
{
    return stdexec::starts_on(exec::inline_scheduler{}, std::forward<Sender>(sender));
}

template <class Sender>
decltype(auto) sync_wait(Sender&& sender)
{
    auto result = stdexec::sync_wait(std::forward<Sender>(sender));
    if constexpr (std::tuple_size_v<typename decltype(result)::value_type> == 1)
    {
        using Opt = std::optional<std::tuple_element_t<0, typename decltype(result)::value_type>>;
        if (result)
        {
            return Opt{std::get<0>(*std::move(result))};
        }
        return Opt{};
    }
    else
    {
        return result;
    }
}

template <class Scope, class Sender, class Scheduler = exec::inline_scheduler>
void scope_spawn_detached(Scope& scope, Sender&& sender, Scheduler&& scheduler = {})
{
    scope.spawn(stdexec::on(std::forward<Scheduler>(scheduler), std::forward<Sender>(sender)));
}

template <class Scope>
decltype(auto) scope_on_empty(Scope& scope)
{
    return scope.on_empty();
}
#endif
}

#endif  // AGRPC_UTILS_EXECUTION_UTILS_HPP
