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

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/execution_context.hpp>

namespace agrpc
{
namespace asio = boost::asio;

namespace detail
{
template <class Executor, class Property, bool = asio::can_query_v<Executor, Property>, class = void>
struct CanQuery : std::false_type
{
};

template <class Executor, class Property>
struct CanQuery<Executor, Property, true, void> : std::true_type
{
    static constexpr decltype(auto) query(const Executor& executor, Property property)
    {
        return asio::query(executor, property);
    }
};

template <class Executor, class Property>
struct CanQuery<Executor, Property, false,
                std::void_t<decltype(std::declval<const Executor&>().query(std::declval<Property>()))>> : std::true_type
{
    static constexpr decltype(auto) query(const Executor& executor, Property property)
    {
        return executor.query(property);
    }
};

template <class Object>
auto get_associated_executor_and_allocator(const Object& object)
{
    auto executor = asio::get_associated_executor(object);
    auto allocator = [&]
    {
        using Querier = detail::CanQuery<decltype(executor), asio::execution::allocator_t<void>>;
        if constexpr (Querier::value)
        {
            return asio::get_associated_allocator(object, Querier::query(executor, asio::execution::allocator));
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
