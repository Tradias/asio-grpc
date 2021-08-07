#ifndef AGRPC_DETAIL_MEMORY_HPP
#define AGRPC_DETAIL_MEMORY_HPP

#include <boost/type_traits/remove_cv_ref.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>

namespace agrpc::detail
{
template <class Allocator>
struct AllocatorDeleter
{
    using allocator_type = boost::remove_cv_ref_t<Allocator>;

    allocator_type allocator;

    template <class T>
    void operator()(T* ptr)
    {
        using Traits = typename std::allocator_traits<allocator_type>::template rebind_traits<T>;
        typename Traits::allocator_type rebound_allocator{std::move(allocator)};
        try
        {
            Traits::destroy(rebound_allocator, ptr);
            Traits::deallocate(rebound_allocator, ptr, 1);
        }
        catch (...)
        {
            Traits::deallocate(rebound_allocator, ptr, 1);
            throw;
        }
    }
};

template <class T, class Allocator, class... Args>
auto allocate_unique(Allocator allocator, Args&&... args)
{
    using Traits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;
    typename Traits::allocator_type alloc{allocator};
    auto* ptr = Traits::allocate(alloc, 1);
    try
    {
        Traits::construct(alloc, ptr, std::forward<Args>(args)...);
    }
    catch (...)
    {
        Traits::deallocate(alloc, ptr, 1);
        throw;
    }
    return std::unique_ptr<T, detail::AllocatorDeleter<Allocator>>{
        ptr, detail::AllocatorDeleter<Allocator>{std::move(allocator)}};
}

template <class T, class Resource>
struct MemoryResourceAllocator
{
    using value_type = T;

    Resource* resource;

    explicit constexpr MemoryResourceAllocator(Resource* resource) noexcept : resource(resource) {}

    template <class U>
    constexpr MemoryResourceAllocator(const MemoryResourceAllocator<U, Resource>& other) noexcept
        : resource(other.resource)
    {
    }

    [[nodiscard]] T* allocate(std::size_t n) { return static_cast<T*>(resource->allocate(n * sizeof(T), alignof(T))); }

    void deallocate(T* p, std::size_t n) noexcept { resource->deallocate(p, n * sizeof(T), alignof(T)); }
};

template <class T, class U, class Resource>
constexpr bool operator==(const detail::MemoryResourceAllocator<T, Resource>& lhs,
                          const detail::MemoryResourceAllocator<U, Resource>& rhs) noexcept
{
    return lhs.resource == rhs.resource;
}

template <class T, class U, class Resource>
constexpr bool operator!=(const detail::MemoryResourceAllocator<T, Resource>& lhs,
                          const detail::MemoryResourceAllocator<U, Resource>& rhs) noexcept
{
    return lhs.resource != rhs.resource;
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_MEMORY_HPP
