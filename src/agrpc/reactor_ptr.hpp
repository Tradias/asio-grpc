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

#ifndef AGRPC_AGRPC_REACTOR_PTR_HPP
#define AGRPC_AGRPC_REACTOR_PTR_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/reactor_executor_base.hpp>
#include <agrpc/detail/reactor_ptr.hpp>
#include <agrpc/detail/reactor_ptr_type.hpp>
#include <agrpc/detail/ref_counted_reactor.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Shared pointer-like object for reactors
 *
 * This smart pointer guarantees that the reactor remains alive until OnDone is called and all user-held objects of this
 * pointer are destroyed.
 *
 * @tparam Reactor The reactor type like `agrpc::ServerUnaryReactor`, `agrpc::ClientUnaryReactor` or a class derived
 * from their base equivalents.
 *
 * @since 3.5.0
 */
template <class Reactor>
class ReactorPtr
{
  private:
    using ValueType = detail::RefCountedReactorTypeT<Reactor>;

  public:
    /**
     * @brief Default constructor
     */
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

    /**
     * @brief Check whether two pointers refer to the same reactor
     */
    [[nodiscard]] friend bool operator==(const ReactorPtr& lhs, const ReactorPtr& rhs) noexcept
    {
        return lhs.ptr_ == rhs.ptr_;
    }

    /**
     * @brief Check whether two pointers do not refer to the same reactor
     */
    [[nodiscard]] friend bool operator!=(const ReactorPtr& lhs, const ReactorPtr& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Get reference to underlying reactor
     */
    [[nodiscard]] Reactor& operator*() const noexcept { return *ptr_; }

    /**
     * @brief Access underlying reactor
     */
    [[nodiscard]] Reactor* operator->() const noexcept { return ptr_; }

    /**
     * @brief Check whether this pointer owns a reactor
     */
    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

    /**
     * @brief Swap the contents of two ReactorPtr
     */
    friend void swap(ReactorPtr& lhs, ReactorPtr& rhs) noexcept { std::swap(lhs.ptr_, rhs.ptr_); }

  private:
    friend detail::ReactorAccess;

    explicit ReactorPtr(ValueType* ptr) noexcept : ptr_(ptr) {}

    ValueType* ptr_{};
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
/**
 * @brief Create ReactorPtr using allocator
 *
 * @tparam Reactor The reactor type like `agrpc::ServerUnaryReactor`, `agrpc::ClientUnaryReactor` or a class derived
 * from their base equivalents.
 *
 * @since 3.5.0
 */
template <class Reactor, class Allocator, class... Args,
          class = std::enable_if_t<!std::is_same_v<void, detail::ReactorExecutorTypeT<Reactor>>>>
[[nodiscard]] inline auto allocate_reactor(Allocator allocator, detail::ReactorExecutorTypeT<Reactor> executor,
                                           Args&&... args)

{
    static_assert(std::is_constructible_v<detail::RefCountedReactorTypeT<Reactor>, Args...>);
    return detail::ReactorAccess::create<ReactorPtr<Reactor>>(allocator, std::move(executor),
                                                              static_cast<Args&&>(args)...);
}
#endif

/**
 * @brief Create ReactorPtr using allocator (sender/receiver overload)
 *
 * @tparam Reactor The reactor type like `agrpc::ServerUnaryReactor`, `agrpc::ClientUnaryReactor` or a class derived
 * from their base equivalents.
 *
 * @since 3.5.0
 */
template <class Reactor, class Allocator, class... Args,
          class = std::enable_if_t<std::is_same_v<void, detail::ReactorExecutorTypeT<Reactor>>>>
[[nodiscard]] inline ReactorPtr<Reactor> allocate_reactor(Allocator allocator, Args&&... args)
{
    static_assert(std::is_constructible_v<detail::RefCountedReactorTypeT<Reactor>, Args...>);
    return detail::ReactorAccess::create<ReactorPtr<Reactor>>(allocator, detail::Empty{}, static_cast<Args&&>(args)...);
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
/**
 * @brief Create ReactorPtr
 *
 * @tparam Reactor The reactor type like `agrpc::ServerUnaryReactor`, `agrpc::ClientUnaryReactor` or a class derived
 * from their base equivalents.
 *
 * @since 3.5.0
 */
template <class Reactor, class... Args,
          class = std::enable_if_t<!std::is_same_v<void, detail::ReactorExecutorTypeT<Reactor>>>>
[[nodiscard]] inline ReactorPtr<Reactor> make_reactor(detail::ReactorExecutorTypeT<Reactor> executor, Args&&... args)
{
    return agrpc::allocate_reactor<Reactor>(std::allocator<void>{}, std::move(executor), static_cast<Args&&>(args)...);
}
#endif

/**
 * @brief Create ReactorPtr (sender/receiver overload)
 *
 * @tparam Reactor The reactor type like `agrpc::ServerUnaryReactor`, `agrpc::ClientUnaryReactor` or a class derived
 * from their base equivalents.
 *
 * @since 3.5.0
 */
template <class Reactor, class... Args,
          class = std::enable_if_t<std::is_same_v<void, detail::ReactorExecutorTypeT<Reactor>>>>
[[nodiscard]] inline ReactorPtr<Reactor> make_reactor(Args&&... args)
{
    return agrpc::allocate_reactor<Reactor>(std::allocator<void>{}, static_cast<Args&&>(args)...);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REACTOR_PTR_HPP
