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

#ifndef AGRPC_DETAIL_UNBIND_HPP
#define AGRPC_DETAIL_UNBIND_HPP

#include <agrpc/bind_allocator.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler, class Executor
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
          ,
          class CancellationSlot
#endif
          >
struct UnbindResult
{
    using CompletionHandlerT = CompletionHandler;

    CompletionHandler completion_handler_;
    Executor executor_;
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    CancellationSlot cancellation_slot_;
#endif

    UnbindResult(CompletionHandler&& completion_handler, Executor&& executor
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
                 ,
                 CancellationSlot&& cancellation_slot
#endif
                 )
        : completion_handler_(std::move(completion_handler)),
          executor_(std::move(executor))
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
          ,
          cancellation_slot_(std::move(cancellation_slot))
#endif
    {
    }

    auto& completion_handler() noexcept { return completion_handler_; }

    auto& executor() noexcept { return executor_; }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    auto& cancellation_slot() noexcept { return cancellation_slot_; }
#endif
};

template <class CompletionHandler>
decltype(auto) unbind_recursively(CompletionHandler&& completion_handler)
{
    return std::forward<CompletionHandler>(completion_handler);
}

template <class CompletionHandler, class Executor>
decltype(auto) unbind_recursively(asio::executor_binder<CompletionHandler, Executor>&& binder);

template <class CompletionHandler, class Allocator>
auto unbind_recursively(agrpc::AllocatorBinder<CompletionHandler, Allocator>&& binder);

#ifdef AGRPC_ASIO_HAS_BIND_ALLOCATOR
template <class CompletionHandler, class Allocator>
auto unbind_recursively(asio::allocator_binder<CompletionHandler, Allocator>&& binder);
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
template <class CompletionHandler, class CancellationSlot>
decltype(auto) unbind_recursively(asio::cancellation_slot_binder<CompletionHandler, CancellationSlot>&& binder)
{
    return detail::unbind_recursively(std::move(binder.get()));
}
#endif

template <class CompletionHandler, class Executor>
decltype(auto) unbind_recursively(asio::executor_binder<CompletionHandler, Executor>&& binder)
{
    return detail::unbind_recursively(std::move(binder.get()));
}

template <class CompletionHandler, class Allocator>
auto unbind_recursively(agrpc::AllocatorBinder<CompletionHandler, Allocator>&& binder)
{
    return agrpc::AllocatorBinder(binder.get_allocator(), detail::unbind_recursively(std::move(binder.get())));
}

#ifdef AGRPC_ASIO_HAS_BIND_ALLOCATOR
template <class CompletionHandler, class Allocator>
auto unbind_recursively(asio::allocator_binder<CompletionHandler, Allocator>&& binder)
{
    return asio::bind_allocator(binder.get_allocator(), detail::unbind_recursively(std::move(binder.get())));
}
#endif

template <class CompletionHandler>
auto unbind_and_get_associates(CompletionHandler&& completion_handler)
{
    auto executor = detail::exec::get_executor(completion_handler);
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
    return UnbindResult{detail::unbind_recursively(std::forward<CompletionHandler>(completion_handler)),
                        std::move(executor), std::move(cancellation_slot)};
#else
    return UnbindResult{detail::unbind_recursively(std::forward<CompletionHandler>(completion_handler)),
                        std::move(executor)};
#endif
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_UNBIND_HPP
