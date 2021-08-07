#ifndef AGRPC_DETAIL_ASIOFORWARD_HPP
#define AGRPC_DETAIL_ASIOFORWARD_HPP

#include <boost/asio.hpp>

namespace agrpc
{
namespace asio = boost::asio;

namespace detail
{
template <class Object>
auto get_associated_executor_and_allocator(const Object& object)
{
    auto executor = asio::get_associated_executor(object);
    auto allocator = [&]
    {
        // TODO C++17
        if constexpr (asio::can_query_v<decltype(executor), asio::execution::allocator_t<std::allocator<void>>>)
        {
            return asio::get_associated_allocator(object, asio::query(executor, asio::execution::allocator));
        }
        else
        {
            return asio::get_associated_allocator(object);
        }
    }();
    return std::pair{std::move(executor), std::move(allocator)};
}
}  // namespace detail
}  // namespace agrpc

#endif  // AGRPC_DETAIL_ASIOFORWARD_HPP
