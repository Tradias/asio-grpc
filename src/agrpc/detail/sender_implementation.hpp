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

#ifndef AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP

#include <agrpc/detail/operation_base.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Initiation, class Implementation>
auto get_stop_function_arg(const Initiation& initiation, Implementation& implementation)
    -> decltype(initiation.stop_function_arg(implementation))
{
    return initiation.stop_function_arg(implementation);
}

template <class Initiation, class Implementation>
auto get_stop_function_arg(const Initiation& initiation, const Implementation&)
    -> decltype(initiation.stop_function_arg())
{
    return initiation.stop_function_arg();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP
