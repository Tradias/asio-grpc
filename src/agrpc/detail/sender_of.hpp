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

#ifndef AGRPC_DETAIL_SENDEROF_HPP
#define AGRPC_DETAIL_SENDEROF_HPP

#include <agrpc/detail/config.hpp>

#include <exception>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class... Values>
struct SenderOf
{
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = Variant<Tuple<Values...>>;

    template <template <class...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    static constexpr bool sends_done = true;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDEROF_HPP
