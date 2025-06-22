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

#ifndef AGRPC_DETAIL_FUNCTIONAL_HPP
#define AGRPC_DETAIL_FUNCTIONAL_HPP

#include <agrpc/detail/utility.hpp>

#include <functional>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Function, class... Args>
decltype(auto) invoke(Function&& function, Args&&... args)
{
    if constexpr (std::is_same_v<void, decltype(std::invoke(static_cast<Function&&>(function),
                                                            static_cast<Args&&>(args)...))>)
    {
        std::invoke(static_cast<Function&&>(function), static_cast<Args&&>(args)...);
        return detail::Empty{};
    }
    else
    {
        return std::invoke(static_cast<Function&&>(function), static_cast<Args&&>(args)...);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_FUNCTIONAL_HPP
