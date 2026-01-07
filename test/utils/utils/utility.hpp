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

#ifndef AGRPC_UTILS_UTILITY_HPP
#define AGRPC_UTILS_UTILITY_HPP

#include <utility>

namespace test
{
template <class T>
struct TypeIdentity
{
    using type = T;
};

template <class T>
using TypeIdentityT = typename TypeIdentity<T>::type;

template <class UseMove, class T>
auto&& move_if(T&& t)
{
    if constexpr (UseMove::value)
    {
        return std::move(t);
    }
    else
    {
        return t;
    }
}

struct AlwaysTrue
{
    bool operator()() const { return true; }
};
}  // namespace test

#endif  // AGRPC_UTILS_UTILITY_HPP
