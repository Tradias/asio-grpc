// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_CANCELABLE_WAITER_HPP
#define AGRPC_DETAIL_CANCELABLE_WAITER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ExecutorOrIoObject>
decltype(auto) get_executor_from_io_object(ExecutorOrIoObject&& exec_or_io_object)
{
    if constexpr (exec::scheduler<detail::RemoveCrefT<ExecutorOrIoObject>>)
    {
        return (exec_or_io_object);
    }
    else if constexpr (exec::scheduler_provider<ExecutorOrIoObject&&>)
    {
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
        return static_cast<ExecutorOrIoObject&&>(exec_or_io_object).get_executor();
#else
        return exec::get_scheduler(static_cast<ExecutorOrIoObject&&>(exec_or_io_object));
#endif
    }
    else
    {
        return exec::get_executor(exec_or_io_object);
    }
}

template <class Signature>
class WaiterCompletionHandler;

template <class... Args>
class WaiterCompletionHandler<void(Args...)>
{
  public:
    void operator()(Args... args) { event_.set(static_cast<Args&&>(args)...); }

  private:
    using Event = detail::ManualResetEvent<void(Args...)>;

    template <class, class>
    friend class agrpc::Waiter;

    explicit WaiterCompletionHandler(Event& event) noexcept : event_(event) {}

    Event& event_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CANCELABLE_WAITER_HPP
