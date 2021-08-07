#ifndef AGRPC_UTILS_ASIOUTILS_HPP
#define AGRPC_UTILS_ASIOUTILS_HPP

#include <boost/asio/co_spawn.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <type_traits>

namespace agrpc::test
{
template <class Function, class Allocator>
struct HandlerWithAssociatedAllocator
{
    using allocator_type = Allocator;

    Function function;
    Allocator allocator;

    auto get_allocator() const noexcept { return allocator; }

    auto operator()() { return function(); }
};

template <class Function, class Allocator>
HandlerWithAssociatedAllocator(Function&&, Allocator&&)
    -> HandlerWithAssociatedAllocator<boost::remove_cv_ref_t<Function>, Allocator>;

template <class Executor, class Function>
auto co_spawn(Executor&& executor, Function function)
{
    return boost::asio::co_spawn(std::forward<Executor>(executor), std::move(function),
                                 [](std::exception_ptr ep, auto&&...)
                                 {
                                     if (ep)
                                     {
                                         std::rethrow_exception(ep);
                                     }
                                 });
}
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_ASIOUTILS_HPP
