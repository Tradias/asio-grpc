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

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/cancel_safe.hpp>
#include <agrpc/grpc_stream.hpp>
#include <agrpc/repeatedly_request.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>

#include <cstddef>

#if defined(AGRPC_ASIO_HAS_CANCELLATION_SLOT) && defined(AGRPC_ASIO_HAS_CO_AWAIT)
#ifdef AGRPC_TEST_ASIO_HAS_FIXED_DEFERRED
TEST_CASE_FIXTURE(test::GrpcContextTest, "CancelSafe: co_await for a CancelSafe and an alarm parallel_group")
{
    test::co_spawn_and_run(grpc_context,
                           [&]() -> asio::awaitable<void>
                           {
                               agrpc::GrpcCancelSafe safe;
                               grpc::Alarm alarm1;
                               agrpc::wait(alarm1, test::five_hundred_milliseconds_from_now(),
                                           asio::bind_executor(grpc_context, safe.token()));
                               grpc::Alarm alarm2;
                               for (size_t i = 0; i < 3; ++i)
                               {
                                   auto [completion_order, alarm2_ok, alarm1_ec, alarm1_ok] =
                                       co_await asio::experimental::make_parallel_group(
                                           agrpc::wait(alarm2, test::ten_milliseconds_from_now(),
                                                       asio::bind_executor(grpc_context, test::ASIO_DEFERRED)),
                                           safe.wait(test::ASIO_DEFERRED))
                                           .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
                                   CHECK_EQ(0, completion_order[0]);
                                   CHECK_EQ(1, completion_order[1]);
                                   CHECK(alarm2_ok);
                                   CHECK_EQ(asio::error::operation_aborted, alarm1_ec);
                                   CHECK_FALSE(alarm1_ok);
                               }
                               CHECK(co_await safe.wait(agrpc::DefaultCompletionToken{}));
                           });
}
#endif

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcStream: next can be interrupted without cancelling initiated operation")
{
    test::co_spawn_and_run(grpc_context,
                           [&]() -> asio::awaitable<void>
                           {
                               agrpc::GrpcStream stream{grpc_context};
                               grpc::Alarm alarm;
                               stream.initiate(agrpc::wait, alarm, test::hundred_milliseconds_from_now());
                               grpc::Alarm alarm2;
                               using namespace asio::experimental::awaitable_operators;
                               auto result =
                                   co_await (agrpc::wait(alarm2, test::ten_milliseconds_from_now()) || stream.next());
                               CHECK_EQ(0, result.index());
                               if (stream.is_running())
                               {
                                   CHECK(co_await stream.next());
                               }
                               co_await stream.cleanup();
                           });
}

auto get_feed_for_topic(int32_t id)
{
    test::v1::Feed feed;
    if (0 == id)
    {
        feed.set_content("zero");
    }
    if (1 == id)
    {
        feed.set_content("one");
    }
    if (2 == id)
    {
        feed.set_content("two");
    }
    return feed;
}

asio::awaitable<void> handle_topic_subscription(
    agrpc::GrpcContext& grpc_context, grpc::ServerContext&,
    grpc::ServerAsyncReaderWriter<test::v1::Feed, test::v1::Topic>& reader_writer)
{
    grpc::Alarm alarm;
    agrpc::GrpcStream read_stream{grpc_context};
    agrpc::GrpcStream write_stream{grpc_context};
    test::v1::Topic topic;
    const auto initiate_write = [&]
    {
        if (!write_stream.is_running())
        {
            write_stream.initiate(agrpc::write, reader_writer, get_feed_for_topic(topic.id()));
        }
    };
    CHECK(co_await read_stream.initiate(agrpc::read, reader_writer, topic).next());
    initiate_write();
    read_stream.initiate(agrpc::read, reader_writer, topic);
    std::chrono::system_clock::time_point deadline;
    const auto update_deadline = [&]
    {
        deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(100);
    };
    update_deadline();
    bool read_ok{true};
    bool write_ok{true};
    do
    {
        using namespace asio::experimental::awaitable_operators;
        const auto variant = co_await (read_stream.next() || agrpc::wait(alarm, deadline) || write_stream.next());
        if (0 == variant.index())  // read completed
        {
            read_ok = std::get<0>(variant);
            if (read_ok)
            {
                read_stream.initiate(agrpc::read, reader_writer, topic);
                update_deadline();
                initiate_write();
            }
        }
        else if (1 == variant.index())  // wait completed
        {
            update_deadline();
            initiate_write();
        }
        else  // write completed
        {
            write_ok = std::get<2>(variant);
        }
    } while (read_ok && write_ok);
    co_await read_stream.cleanup();
    co_await write_stream.cleanup();
    co_await agrpc::finish(reader_writer, grpc::Status::OK);
}

void register_subscription_handler(agrpc::GrpcContext& grpc_context, test::v1::Test::AsyncService& service)
{
    agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestSubscribe, service,
                              asio::bind_executor(grpc_context,
                                                  [&](auto&&... args)
                                                  {
                                                      return handle_topic_subscription(
                                                          grpc_context, std::forward<decltype(args)>(args)...);
                                                  }));
}

asio::awaitable<void> make_topic_subscription_request(agrpc::GrpcContext& grpc_context, test::v1::Test::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(40));

    std::unique_ptr<grpc::ClientAsyncReaderWriter<test::v1::Topic, test::v1::Feed>> reader_writer;
    CHECK(co_await agrpc::request(&test::v1::Test::Stub::PrepareAsyncSubscribe, stub, client_context, reader_writer));

    test::v1::Topic topic;
    test::v1::Feed feed;
    grpc::Alarm alarm;
    agrpc::GrpcStream read_stream{grpc_context};

    bool read_ok{true};
    for (int32_t topic_id{}; topic_id < 3; ++topic_id)
    {
        topic.set_id(topic_id);
        bool write_ok = co_await agrpc::write(reader_writer, topic);

        read_stream.initiate(agrpc::read, reader_writer, feed);

        const auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(333);
        while (read_ok && write_ok)
        {
            using namespace asio::experimental::awaitable_operators;
            const auto variant = co_await (read_stream.next() || agrpc::wait(alarm, deadline));
            if (0 == variant.index())  // read completed
            {
                read_ok = std::get<0>(variant);
                if (read_ok)
                {
                    std::cout << feed.content() << std::endl;
                    read_stream.initiate(agrpc::read, reader_writer, feed);
                }
            }
            else  // alarm completed
            {
                co_await read_stream.cleanup();
                break;
            }
        }
        feed.Clear();
    }
    CHECK(co_await agrpc::writes_done(reader_writer));

    co_await read_stream.cleanup();

    client_context.TryCancel();

    grpc::Status status;
    co_await agrpc::finish(reader_writer, status);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "GrpcStream bidi streaming example")
{
    test::ServerShutdownInitiator shutdown{*server};
    register_subscription_handler(grpc_context, service);
    test::co_spawn_and_run(grpc_context,
                           [&]() -> asio::awaitable<void>
                           {
                               co_await make_topic_subscription_request(grpc_context, *stub);
                               shutdown.initiate();
                           });
}
#endif
