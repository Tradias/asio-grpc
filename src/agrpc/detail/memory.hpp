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

#ifndef AGRPC_DETAIL_MEMORY_HPP
#define AGRPC_DETAIL_MEMORY_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>

#include <cstddef>
#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
struct UnwrapUniquePtr
{
    using Type = T;
};

template <class T>
struct UnwrapUniquePtr<std::unique_ptr<T>>
{
    using Type = T;
};

template <class T>
struct UnwrapUniquePtr<const std::unique_ptr<T>>
{
    using Type = T;
};

template <class T>
using UnwrapUniquePtrT = typename detail::UnwrapUniquePtr<T>::Type;

template <class T>
auto& unwrap_unique_ptr(T& t) noexcept
{
    return t;
}

template <class T>
auto& unwrap_unique_ptr(std::unique_ptr<T>& t) noexcept
{
    return *t;
}

template <class T>
auto& unwrap_unique_ptr(const std::unique_ptr<T>& t) noexcept
{
    return *t;
}

template <class T, class... Args>
T* construct_at(T* p, Args&&... args)
{
    return ::new (const_cast<void*>(static_cast<const void*>(p))) T(static_cast<Args&&>(args)...);
}

template <std::size_t Size>
class StackBuffer
{
  public:
    static constexpr auto SIZE = Size;

    template <class T>
    auto assign(T&& t)
    {
        using Tunref = detail::RemoveCrefT<T>;
        static_assert(alignof(std::max_align_t) >= alignof(Tunref), "Overaligned types are not supported");
        return detail::construct_at(reinterpret_cast<Tunref*>(&buffer_), static_cast<T&&>(t));
    }

    void* get() noexcept { return buffer_; }

  private:
    alignas(std::max_align_t) std::byte buffer_[Size];
};

class DelayedBuffer
{
  private:
    struct alignas(std::max_align_t) Buffer
    {
        std::byte data;
    };

  public:
    template <class T>
    auto assign(T&& t)
    {
        using Tunref = detail::RemoveCrefT<T>;
        static_assert(alignof(std::max_align_t) >= alignof(Tunref), "Overaligned types are not supported");
        if (buffer)
        {
            return detail::construct_at(get<Tunref>(), static_cast<T&&>(t));
        }
        else
        {
            buffer = std::make_unique<Buffer[]>(sizeof(Tunref));
            return detail::construct_at(get<Tunref>(), static_cast<T&&>(t));
        }
    }

    template <class T = void>
    T* get() noexcept
    {
        return reinterpret_cast<T*>(buffer.get());
    }

  private:
    std::unique_ptr<Buffer[]> buffer;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_HPP
