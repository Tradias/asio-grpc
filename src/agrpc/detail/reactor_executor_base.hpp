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

#ifndef AGRPC_DETAIL_REACTOR_EXECUTOR_BASE_HPP
#define AGRPC_DETAIL_REACTOR_EXECUTOR_BASE_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Reactor's executor base
 *
 * @since 3.5.0
 */
template <class Executor>
class ReactorExecutorBase
{
  public:
    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    ReactorExecutorBase() {}

    ~ReactorExecutorBase() {}

    /**
     * @brief Get the executor
     *
     * Thread-safe
     */
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
  protected:
    [[nodiscard]] detail::Empty get_executor() const noexcept { return {}; }
};

struct ReactorExecutorType
{
    template <class Executor>
    static Executor get(ReactorExecutorBase<Executor>*);
};

template <class Reactor>
using ReactorExecutorTypeT = decltype(ReactorExecutorType::get(static_cast<Reactor*>(nullptr)));

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
using DefaultReactorExecutor = asio::any_io_executor;
#else
using DefaultReactorExecutor = void;
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REACTOR_EXECUTOR_BASE_HPP
