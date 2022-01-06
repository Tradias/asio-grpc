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

#ifndef AGRPC_HELPER_COSPAWNER_HPP
#define AGRPC_HELPER_COSPAWNER_HPP

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

template <class Handler>
struct CoSpawner
{
    using executor_type = boost::asio::associated_executor_t<Handler>;
    using allocator_type = boost::asio::associated_allocator_t<Handler>;

    Handler handler;

    explicit CoSpawner(Handler handler) : handler(std::move(handler)) {}

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& request_context, bool request_ok) &&
    {
        if (!request_ok)
        {
            return;
        }
        boost::asio::co_spawn(
            this->get_executor(),
            [handler = std::move(handler),
             request_context = std::move(request_context)]() mutable -> boost::asio::awaitable<void>
            {
                co_await std::apply(std::move(handler), request_context.args());
            },
            boost::asio::detached);
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return boost::asio::get_associated_executor(handler); }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler);
    }
};

#endif  // AGRPC_HELPER_COSPAWNER_HPP
