#ifndef AGRPC_UTILS_ASIOUTILS_HPP
#define AGRPC_UTILS_ASIOUTILS_HPP

#include <boost/type_traits/remove_cv_ref.hpp>

#include <type_traits>
#include <version>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/co_spawn.hpp>
#endif

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
    -> HandlerWithAssociatedAllocator<boost::remove_cv_ref_t<Function>, boost::remove_cv_ref_t<Allocator>>;

#ifdef BOOST_ASIO_HAS_CO_AWAIT
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
#endif
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_ASIOUTILS_HPP
