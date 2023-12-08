// Copyright 2023 Dennis Hezel
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

#include <agrpc/detail/config.hpp>
#include <exec/inline_scheduler.hpp>
#include <stdexec/execution.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail::exec
{
using ::stdexec::get_allocator;
using ::stdexec::get_scheduler;
inline const auto& get_executor = get_scheduler;
using ::stdexec::scheduler;

template <class, class = void>
inline constexpr bool scheduler_provider = false;

template <class T>
inline constexpr bool scheduler_provider<T, decltype((void)std::declval<T>().get_scheduler())> = true;

template <class T>
inline constexpr bool is_sender_v = ::stdexec::sender<T>;

using ::exec::inline_scheduler;
using ::stdexec::connect;
using ::stdexec::connect_result_t;
using ::stdexec::get_stop_token;

template <class Receiver>
void set_done(Receiver&& receiver)
{
    ::stdexec::set_stopped(static_cast<Receiver&&>(receiver));
}

using ::stdexec::set_error;
using ::stdexec::set_value;
using ::stdexec::start;

template <class T>
using stop_token_type_t = ::stdexec::stop_token_of_t<T>;

using ::stdexec::tag_t;
}  // namespace exec

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_EXECUTION_STDEXEC_HPP
