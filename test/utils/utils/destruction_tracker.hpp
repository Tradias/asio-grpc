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

#ifndef AGRPC_UTILS_DESTRUCTION_TRACKER_HPP
#define AGRPC_UTILS_DESTRUCTION_TRACKER_HPP

#include <memory>

namespace test
{
struct DestructionTracker
{
    explicit DestructionTracker(bool& destructed) noexcept : destructed(destructed) {}

    ~DestructionTracker() noexcept { destructed = true; }

    static auto make(bool& destructed) { return std::make_unique<DestructionTracker>(destructed); }

    bool& destructed;
};
}

#endif  // AGRPC_UTILS_DESTRUCTION_TRACKER_HPP
