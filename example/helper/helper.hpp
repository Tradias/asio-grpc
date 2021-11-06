// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_EXAMPLE_HELPER_H
#define AGRPC_EXAMPLE_HELPER_H

#include <cstdlib>

void abort_if_not(bool condition)
{
    if (!condition)
    {
        std::abort();
    }
}
template <class... Args>
void silence_unused(Args&&... args)
{
    ((void)args, ...);
}

#endif  // AGRPC_EXAMPLE_HELPER_H
