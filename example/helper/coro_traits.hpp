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

#ifndef AGRPC_HELPER_CORO_TRAITS_HPP
#define AGRPC_HELPER_CORO_TRAITS_HPP

#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/coro.hpp>

namespace example
{
/* [asio-coro-traits] */
template <class Executor = boost::asio::any_io_executor, class Allocator = std::allocator<void>>
struct AsioCoroTraits
{
    using ReturnType = boost::asio::experimental::coro<void, void, Executor, Allocator>;

    template <class RPCHandler, class CompletionHandler>
    static boost::asio::deferred_t completion_token(RPCHandler&, CompletionHandler&)
    {
        return {};
    }

    template <class RPCHandler, class CompletionHandler, class IoExecutor, class Function>
    static void co_spawn(const IoExecutor& io_executor, RPCHandler&, CompletionHandler& completion_handler,
                         Function&& function)
    {
        boost::asio::experimental::co_spawn(
            static_cast<Function&&>(function)(boost::asio::get_associated_executor(completion_handler, io_executor)),
            boost::asio::detached);
    }
};
/* [asio-coro-traits] */
}  // namespace example

#endif  // AGRPC_HELPER_CORO_TRAITS_HPP
