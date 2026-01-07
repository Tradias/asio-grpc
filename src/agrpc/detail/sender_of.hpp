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

#ifndef AGRPC_DETAIL_SENDER_OF_HPP
#define AGRPC_DETAIL_SENDER_OF_HPP

#include <exception>

#include <agrpc/detail/config.hpp>

#ifdef AGRPC_STDEXEC
#include <stdexec/execution.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
struct SenderOf;

template <class... Values>
struct SenderOf<void(Values...)>
{
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = Variant<Tuple<Values...>>;

    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    static constexpr bool sends_done = true;

    using is_sender = void;

#ifdef AGRPC_STDEXEC
    using completion_signatures =
        stdexec::completion_signatures<stdexec::set_value_t(Values...), stdexec::set_error_t(std::exception_ptr),
                                       stdexec::set_stopped_t()>;
#endif
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_OF_HPP
