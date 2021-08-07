// Copyright 2021 Dennis Hezel
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
