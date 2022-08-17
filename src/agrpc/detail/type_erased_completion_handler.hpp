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

#ifndef AGRPC_DETAIL_TYPE_ERASED_COMPLETION_HANDLER_HPP
#define AGRPC_DETAIL_TYPE_ERASED_COMPLETION_HANDLER_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/void_pointer_traits.hpp>

#include <cassert>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CompletionHandler>
CompletionHandler deallocate_completion_handler(CompletionHandler* completion_handler) noexcept
{
    auto local_completion_handler{static_cast<CompletionHandler&&>(*completion_handler)};
    detail::destroy_deallocate(completion_handler, asio::get_associated_allocator(local_completion_handler));
    return local_completion_handler;
}

template <class CompletionHandler, class... Args>
void deallocate_and_invoke(void* data, Args... args)
{
    auto* completion_handler = static_cast<CompletionHandler*>(data);
    auto local_completion_handler = detail::deallocate_completion_handler(completion_handler);
    static_cast<CompletionHandler&&>(local_completion_handler)(static_cast<Args&&>(args)...);
}

template <class Signature, class VoidPointer>
class BasicTypeErasedCompletionHandler;

template <class Signature>
using AtomicTypeErasedCompletionHandler = detail::BasicTypeErasedCompletionHandler<Signature, std::atomic<void*>>;

template <class Signature>
using TypeErasedCompletionHandler = detail::BasicTypeErasedCompletionHandler<Signature, void*>;

template <class VoidPointer, class... Args>
class BasicTypeErasedCompletionHandler<void(Args...), VoidPointer>
{
  private:
    using Complete = void (*)(void*, Args...);
    using VoidPointerTraits = detail::VoidPointerTraits<VoidPointer>;

  public:
    BasicTypeErasedCompletionHandler() = default;

    BasicTypeErasedCompletionHandler(const BasicTypeErasedCompletionHandler&) = delete;
    BasicTypeErasedCompletionHandler(BasicTypeErasedCompletionHandler&&) = delete;
    BasicTypeErasedCompletionHandler& operator=(const BasicTypeErasedCompletionHandler&) = delete;
    BasicTypeErasedCompletionHandler& operator=(BasicTypeErasedCompletionHandler&&) = delete;

#ifndef NDEBUG
    ~BasicTypeErasedCompletionHandler()
    {
        assert(!(*this) && "Forgot to wait for an asynchronous operation to complete?");
    }
#endif

    template <class Target, class CompletionHandler>
    void emplace(CompletionHandler&& ch)
    {
        auto allocator = asio::get_associated_allocator(ch);
        completion_handler = detail::allocate<Target>(allocator, static_cast<CompletionHandler&&>(ch)).release();
        complete_ = &detail::deallocate_and_invoke<Target, Args...>;
    }

    detail::TypeErasedCompletionHandler<void(Args...)> release() noexcept
    {
        return {this->release_completion_handler(), complete_};
    }

    explicit operator bool() const noexcept { return static_cast<bool>(completion_handler); }

    template <class... CompletionArgs>
    void complete(CompletionArgs&&... args) &&
    {
        complete_(this->release_completion_handler(), static_cast<CompletionArgs&&>(args)...);
    }

  private:
    template <class, class>
    friend class detail::BasicTypeErasedCompletionHandler;

    BasicTypeErasedCompletionHandler(void* completion_handler, Complete complete)
        : completion_handler(completion_handler), complete_(complete)
    {
    }

    void* release_completion_handler() noexcept { return VoidPointerTraits::exchange(completion_handler, nullptr); }

    VoidPointer completion_handler{};
    Complete complete_;
};
}
AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_TYPE_ERASED_COMPLETION_HANDLER_HPP
