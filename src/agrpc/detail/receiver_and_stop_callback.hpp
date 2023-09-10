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

#ifndef AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP
#define AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>

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

template <class StopFunction, class Operation, class GetStopFunctionArg>
void emplace_stop_callback(Operation& operation, GetStopFunctionArg get_stop_function_arg)
{
    using CompletionHandler = detail::RemoveCrefT<decltype(operation.completion_handler())>;
    if constexpr (detail::NEEDS_STOP_CALLBACK<exec::stop_token_type_t<CompletionHandler&>, StopFunction>)
    {
        auto stop_token = exec::get_stop_token(operation.completion_handler());
        if (detail::stop_possible(stop_token))
        {
            stop_token.template emplace<StopFunction>(get_stop_function_arg());
        }
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP
