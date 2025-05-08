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

#ifndef AGRPC_DETAIL_REACTOR_EXECUTOR_BASE_HPP
#define AGRPC_DETAIL_REACTOR_EXECUTOR_BASE_HPP

#include <agrpc/detail/forward.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Executor>
class ReactorExecutorBase
{
  public:
    using executor_type = Executor;

    ReactorExecutorBase() {}

    ~ReactorExecutorBase() {}

    [[nodiscard]] const Executor& get_executor() const noexcept { return executor_; }

  private:
    friend detail::ReactorAccess;

    union
    {
        Executor executor_;
    };
};

template <>
class ReactorExecutorBase<void>
{
};

struct ReactorExecutorType
{
    template <class Executor>
    static Executor get(ReactorExecutorBase<Executor>*);
};

template <class Reactor>
using ReactorExecutorTypeT = decltype(ReactorExecutorType::get(static_cast<Reactor*>(nullptr)));
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REACTOR_EXECUTOR_BASE_HPP
