// Copyright 2023 Dennis Hezel
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

#include "example/v1/example.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <cassert>

namespace asio = boost::asio;

/* [waiter-example] */
using ServerRPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestBidirectionalStreaming>;

asio::awaitable<void> request_handler_using_waiter(ServerRPC& rpc)
{
    ServerRPC::Request request;
    ServerRPC::Response response;

    agrpc::Alarm alarm{rpc.get_executor()};

    agrpc::Waiter<void(bool)> waiter;
    waiter.initiate(agrpc::read, rpc, request);

    auto next_deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);

    // Read requests from the client and send a response back every five seconds
    while (true)
    {
        auto [completion_order, read_error_code, read_ok, alarm_expired] =
            co_await asio::experimental::make_parallel_group(waiter.wait(asio::deferred),
                                                             alarm.wait(next_deadline, asio::deferred))
                .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
        if (0 == completion_order[0])  // read completed
        {
            if (!read_ok)
            {
                co_return;
            }
            waiter.initiate(agrpc::read, rpc, request);
        }
        else  // alarm expired
        {
            co_await rpc.write(response, asio::use_awaitable);
            next_deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        }
    }
}
/* [waiter-example] */
