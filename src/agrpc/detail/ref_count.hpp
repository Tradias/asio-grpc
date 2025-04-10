// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_REF_COUNT_HPP
#define AGRPC_DETAIL_REF_COUNT_HPP

#include <agrpc/detail/utility.hpp>

#include <atomic>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class RefCount
{
  public:
    explicit RefCount(std::size_t initial_count = 1) noexcept : reference_count_{initial_count} {}

    void increment() noexcept { ++reference_count_; }

    [[nodiscard]] bool decrement() noexcept { return 0 == --reference_count_; }

  private:
    std::atomic_size_t reference_count_;
};

template <class Self>
struct RefCountGuardFn
{
    void operator()() { self_.decrement_ref_count(); }

    Self& self_;
};

template <class Self>
using RefCountGuard = detail::ScopeGuard<RefCountGuardFn<Self>>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REF_COUNT_HPP
