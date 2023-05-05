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

#ifndef AGRPC_DETAIL_EXECUTION_UNIFEX_HPP
#define AGRPC_DETAIL_EXECUTION_UNIFEX_HPP

#include <agrpc/detail/config.hpp>
#include <unifex/config.hpp>
#include <unifex/get_allocator.hpp>
#include <unifex/get_stop_token.hpp>
#include <unifex/inline_scheduler.hpp>
#include <unifex/receiver_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/stop_token_concepts.hpp>
#include <unifex/tag_invoke.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail::exec
{
using ::unifex::get_allocator;
using ::unifex::get_scheduler;
inline const auto& get_executor = get_scheduler;

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
using ::unifex::stop_token_type_t;
using ::unifex::tag_t;
using ::unifex::unstoppable_token;
}  // namespace exec

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_EXECUTION_UNIFEX_HPP
