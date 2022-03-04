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

#ifndef AGRPC_AGRPC_BINDALLOCATOR_HPP
#define AGRPC_AGRPC_BINDALLOCATOR_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/utility.hpp"

AGRPC_NAMESPACE_BEGIN()

template <class Target, class Allocator>
class AllocatorBinder
{
  public:
    using target_type = Target;
    using executor_type = asio::associated_executor_t<Target>;
    using allocator_type = Allocator;

    template <class... Args>
    explicit AllocatorBinder(const Allocator& allocator, Args&&... args)
        : allocator(allocator), target(std::forward<Args>(args)...)
    {
    }

    AllocatorBinder(const AllocatorBinder& other) = default;

    template <class OtherTarget, class OtherAllocator>
    explicit AllocatorBinder(const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : allocator(other.get_allocator()), target(other.get())
    {
    }

    template <class OtherTarget, class OtherAllocator>
    AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : allocator(allocator), target(other.get())
    {
    }

    template <class OtherTarget, class OtherAllocator>
    AllocatorBinder(const Allocator& allocator, const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : allocator(allocator), target(other.get())
    {
    }

    AllocatorBinder(AllocatorBinder&& other) = default;

    template <class OtherTarget, class OtherAllocator>
    explicit AllocatorBinder(AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : allocator(std::move(other.get_allocator())), target(std::move(other.get()))
    {
    }

    template <class OtherTarget, class OtherAllocator>
    AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : allocator(allocator), target(std::move(other.get()))
    {
    }

    ~AllocatorBinder() = default;

    AllocatorBinder& operator=(const AllocatorBinder& other) = default;
    AllocatorBinder& operator=(AllocatorBinder&& other) = default;

    target_type& get() noexcept { return target; }

    const target_type& get() const noexcept { return target; }

    executor_type get_executor() const noexcept { return asio::get_associated_executor(target); }

    allocator_type get_allocator() const noexcept { return allocator; }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) &&
    {
        return std::move(target)(std::forward<Args>(args)...);
    }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) &
    {
        return target(std::forward<Args>(args)...);
    }

    template <class... Args>
    decltype(auto) operator()(Args&&... args) const&
    {
        return target(std::forward<Args>(args)...);
    }

  private:
    Allocator allocator;
    Target target;
};

template <class Allocator, class Target>
AllocatorBinder(const Allocator& allocator, Target&& target)
    -> AllocatorBinder<detail::RemoveCvrefT<Target>, Allocator>;

template <class Allocator, class Target>
auto bind_allocator(const Allocator& allocator, Target&& target)
{
    return agrpc::AllocatorBinder{allocator, std::forward<Target>(target)};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_BINDALLOCATOR_HPP

#include "agrpc/detail/bindAllocator.ipp"
