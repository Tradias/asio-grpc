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

#ifndef AGRPC_UTILS_DELETE_GUARD_HPP
#define AGRPC_UTILS_DELETE_GUARD_HPP

namespace test
{
struct DeleteGuard
{
    using DeleteFunction = void (*)(void*);

    ~DeleteGuard()
    {
        if (to_delete)
        {
            delete_function(to_delete);
        }
    }

    template <class Factory>
    auto& emplace_with(Factory factory)
    {
        using T = decltype(factory());
        auto* t = new T(factory());
        to_delete = t;
        delete_function = [](void* p)
        {
            delete static_cast<T*>(p);
        };
        return *t;
    }

    void* to_delete{};
    DeleteFunction delete_function{};
};
}  // namespace test

#endif  // AGRPC_UTILS_DELETE_GUARD_HPP
