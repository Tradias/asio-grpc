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

#include "test/v1/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/time.hpp"

#include <agrpc/cancelSafe.hpp>
#include <agrpc/grpcStream.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcContextTest, "CancelSafe: co_await for a CancelSafe and an alarm using operator||")
{
    test::co_spawn(get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       agrpc::GrpcCancelSafe safe;
                       grpc::Alarm alarm;
                       agrpc::wait(alarm, test::five_hundred_milliseconds_from_now(),
                                   asio::bind_executor(grpc_context, safe.token()));
                       grpc::Alarm alarm2;
                       for (size_t i = 0; i < 3; ++i)
                       {
                           auto [completion_order, alarm2_ok, alarm1_ec, alarm1_ok] =
                               co_await asio::experimental::make_parallel_group(
                                   agrpc::wait(alarm2, test::ten_milliseconds_from_now(),
                                               asio::bind_executor(grpc_context, asio::experimental::deferred)),
                                   safe.wait(asio::experimental::deferred))
                                   .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
                           CHECK_EQ(0, completion_order[0]);
                           CHECK_EQ(1, completion_order[1]);
                           CHECK(alarm2_ok);
                           CHECK_EQ(asio::error::operation_aborted, alarm1_ec);
                           CHECK_EQ(bool{}, alarm1_ok);
                       }
                       CHECK(co_await safe.wait(agrpc::DefaultCompletionToken{}));
                   });
    grpc_context.run();
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcStream: next can be interrupted without cancelling initiated operation")
{
    test::co_spawn(get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       agrpc::GrpcStream stream{grpc_context};
                       grpc::Alarm alarm;
                       stream.initiate(agrpc::wait, alarm, test::hundred_milliseconds_from_now());
                       grpc::Alarm alarm2;
                       using namespace asio::experimental::awaitable_operators;
                       auto result = co_await (agrpc::wait(alarm2, test::ten_milliseconds_from_now()) || stream.next());
                       CHECK_EQ(0, result.index());
                       CHECK(co_await stream.next());
                       co_await stream.cleanup();
                   });
    grpc_context.run();
}

// TEST_CASE_FIXTURE(test::GrpcClientServerTest, "CancelSafeStream")
// {
//     test::co_spawn(
//         get_executor(),
//         [&]() -> asio::awaitable<void>
//         {
//             grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request> reader_writer{&server_context};
//             CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, service,
//                                           server_context, reader_writer));
//             test::msg::Request request;
//             CancelSafeStream stream{grpc_context, agrpc::read, reader_writer, request};
// MonotonicResource<150> resour;
// grpc::Alarm alarm;
// auto expected{1};
// const auto next = [&]() -> asio::awaitable<std::variant<bool, bool>>
// {
//     const auto [completion_order, wait_ok, read_ec, read_ok] =
//         co_await asio::experimental::make_parallel_group(
//             agrpc::wait(
//                 alarm, test::ten_milliseconds_from_now(),
//                 agrpc::bind_allocator(resour.get_allocator(),
//                                       asio::bind_executor(get_executor(),
//                                       asio::experimental::deferred))),
//             stream.next(agrpc::bind_allocator(
//                 resour.get_allocator(), asio::bind_executor(get_executor(),
//                 asio::experimental::deferred))))
//             .async_wait(asio::experimental::wait_for_one(),
//                         agrpc::bind_allocator(resour.get_allocator(), asio::use_awaitable));
//     resour.reset();
//     if (0 == completion_order[0])
//     {
//         co_return std::variant<bool, bool>{std::in_place_index<0>, wait_ok};
//     }
//     else
//     {
//         co_return std::variant<bool, bool>{std::in_place_index<1>, read_ok};
//     }
// };
// while (true)
// {
//     auto result = co_await next();
//     if (0 == result.index())
//     {
//         std::cout << "waited\n";
//     }
//     else
//     {
//         if (!std::get<1>(result))
//         {
//             break;
//         }
//         std::cout << "read\n";
//         CHECK(std::get<1>(result));
//         CHECK_EQ(expected, request.integer());
//         ++expected;
//     }
// }
//             grpc::Alarm alarm;
//             auto expected{1};
//             test::msg::Response response;
//             agrpc::GrpcCancelSafe write_safe;
//             using namespace asio::experimental::awaitable_operators;
//             while (true)
//             {
//                 auto result = co_await (agrpc::wait(alarm, test::ten_milliseconds_from_now()) ||
//                                         stream.next(asio::use_awaitable) || write_safe.wait(asio::use_awaitable));
//                 if (0 == result.index())
//                 {
//                     co_await stream.cleanup(asio::use_awaitable);
//                     co_await stream.cleanup(asio::use_awaitable);
//                     break;
//                     std::cout << "waited\n";
//                 }
//                 else if (1 == result.index())
//                 {
//                     if (!std::get<1>(result))
//                     {
//                         break;
//                     }
//                     std::cout << "read " << request.integer() << "\n";
//                     CHECK(std::get<1>(result));
//                     CHECK_EQ(expected, request.integer());
//                     response.set_integer(request.integer() * 10);
//                     agrpc::write(reader_writer, response, asio::bind_executor(grpc_context, write_safe.token()));
//                     ++expected;
//                 }
//                 else if (2 == result.index())
//                 {
//                     if (!std::get<2>(result))
//                     {
//                         co_await stream.cleanup(asio::use_awaitable);
//                         co_await stream.cleanup(asio::use_awaitable);
//                         break;
//                     }
//                     std::cout << "write\n";
//                 }
//             }
//             CHECK(co_await agrpc::finish(reader_writer, grpc::Status::OK));
//         });
//     test::co_spawn(get_executor(),
//                    [&]() -> asio::awaitable<void>
//                    {
//                        auto [writer, ok] = co_await
//                        agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming,
//                                                                    *stub, client_context);
//                        CHECK(ok);
//                        grpc::Alarm alarm;
//                        co_await agrpc::wait(alarm, test::ten_milliseconds_from_now());
//                        test::msg::Request request;
//                        request.set_integer(1);
//                        CHECK(co_await agrpc::write(*writer, request));
//                        co_await agrpc::wait(alarm, test::ten_milliseconds_from_now());
//                        test::msg::Response response;
//                        CHECK(co_await agrpc::read(*writer, response));
//                        CHECK_EQ(10, response.integer());
//                        request.set_integer(2);
//                        CHECK(co_await agrpc::write(*writer, request));
//                        co_await agrpc::wait(alarm, test::ten_milliseconds_from_now());
//                        request.set_integer(3);
//                        CHECK(co_await agrpc::write(*writer, request, grpc::WriteOptions{}));
//                        //    CHECK(co_await agrpc::write_last(*writer, request, grpc::WriteOptions{}));
//                        //    CHECK(co_await agrpc::read(*writer, response));
//                        //    CHECK_EQ(20, response.integer());
//                        //       client_context.TryCancel();
//                        //    CHECK(co_await agrpc::read(*writer, response));
//                        //    CHECK_EQ(30, response.integer());
//                        //    grpc::Status status;
//                        //    CHECK(co_await agrpc::finish(*writer, status));
//                        //    CHECK(status.ok());
//                    });
//     grpc_context.run();
// }
#endif
}
#endif
