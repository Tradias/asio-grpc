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

#ifndef AGRPC_DETAIL_WAITER_HPP
#define AGRPC_DETAIL_WAITER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/asio_utils.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/manual_reset_event.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ExecutorOrIoObject>
decltype(auto) get_executor_from_io_object(ExecutorOrIoObject&& exec_or_io_object)
{
    if constexpr (detail::IS_EXECUTOR<detail::RemoveCrefT<ExecutorOrIoObject>>)
    {
        return (exec_or_io_object);
    }
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    else if constexpr (detail::IS_EXECUTOR_PROVIDER<ExecutorOrIoObject>)
    {
        return static_cast<ExecutorOrIoObject&&>(exec_or_io_object).get_executor();
    }
#if defined(AGRPC_UNIFEX) || defined(AGRPC_STDEXEC)
    else if constexpr (exec::scheduler_provider<ExecutorOrIoObject>)
    {
        return exec::get_scheduler(static_cast<ExecutorOrIoObject&&>(exec_or_io_object));
    }
#endif
    else
    {
        return asio::get_associated_executor(exec_or_io_object);
    }
#else
    else
    {
        return exec::get_scheduler(static_cast<ExecutorOrIoObject&&>(exec_or_io_object));
    }
#endif
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

#endif  // AGRPC_DETAIL_WAITER_HPP
