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

#ifndef AGRPC_AGRPC_SERVER_CALLBACK_PTR_HPP
#define AGRPC_AGRPC_SERVER_CALLBACK_PTR_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/reactor_executor_base.hpp>
#include <agrpc/detail/reactor_ptr.hpp>
#include <agrpc/detail/reactor_ptr_type.hpp>
#include <agrpc/detail/ref_counted_reactor.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

template <class Reactor>
class ReactorPtr
{
  private:
    using Allocation = detail::RefCountedReactorTypeT<Reactor>;
    using Ptr = Allocation*;

  public:
    ReactorPtr() = default;

    ReactorPtr(const ReactorPtr& other) noexcept : ptr_(other.ptr_)
    {
        if (ptr_)
        {
            ptr_->increment_ref_count();
        }
    }

    ReactorPtr(ReactorPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    ~ReactorPtr() noexcept
    {
        if (ptr_)
        {
            ptr_->decrement_ref_count();
        }
    }

    ReactorPtr& operator=(const ReactorPtr& other) noexcept
    {
        if (this != &other)
        {
            if (ptr_)
            {
                ptr_->decrement_ref_count();
            }
            ptr_ = other.ptr_;
            if (ptr_)
            {
                ptr_->increment_ref_count();
            }
        }
        return *this;
    }

    ReactorPtr& operator=(ReactorPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr_)
            {
                ptr_->decrement_ref_count();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] bool operator==(const ReactorPtr& other) const noexcept { return ptr_ == other.ptr_; }

    [[nodiscard]] bool operator!=(const ReactorPtr& other) const noexcept { return !(*this == other); }

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

    [[nodiscard]] Reactor* operator->() const noexcept { return ptr_; }

    [[nodiscard]] Reactor& operator*() const noexcept { return *ptr_; }

  private:
    friend detail::ReactorAccess;

    explicit ReactorPtr(Ptr ptr) noexcept : ptr_(ptr) {}

    Ptr ptr_{};
};

template <class Reactor, class Allocator, class... Args,
          class = std::enable_if_t<!std::is_same_v<void, detail::ReactorExecutorTypeT<Reactor>>>>
[[nodiscard]] inline auto allocate_reactor(Allocator allocator, detail::ReactorExecutorTypeT<Reactor> executor,
                                           Args&&... args)

{
    static_assert(std::is_constructible_v<detail::RefCountedReactorTypeT<Reactor>, Args...>);
    return detail::ReactorAccess::create<ReactorPtr<Reactor>>(allocator, std::move(executor),
                                                              static_cast<Args&&>(args)...);
}

template <class Reactor, class Allocator, class... Args,
          class = std::enable_if_t<std::is_same_v<void, detail::ReactorExecutorTypeT<Reactor>>>>
[[nodiscard]] inline ReactorPtr<Reactor> allocate_reactor(Allocator allocator, Args&&... args)
{
    static_assert(std::is_constructible_v<detail::RefCountedReactorTypeT<Reactor>, Args...>);
    return detail::ReactorAccess::create<ReactorPtr<Reactor>>(allocator, detail::Empty{}, static_cast<Args&&>(args)...);
}

template <class Reactor, class... Args>
[[nodiscard]] inline ReactorPtr<Reactor> make_reactor(typename Reactor::executor_type executor, Args&&... args)
{
    return agrpc::allocate_reactor<Reactor>(std::allocator<void>{}, std::move(executor), static_cast<Args&&>(args)...);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_CALLBACK_PTR_HPP
