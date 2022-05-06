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
#include "utils/grpcContextTest.hpp"
#include "utils/rpc.hpp"

#include <agrpc/repeatedlyRequest.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <thread>
#include <vector>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
struct GrpcRepeatedlyRequestTest : test::GrpcClientServerTest
{
    template <class RPC, class Service, class ServerFunction, class ClientFunction, class Allocator>
    auto test(RPC rpc, Service& service, ServerFunction server_function, ClientFunction client_function,
              Allocator allocator)
    {
        agrpc::repeatedly_request(rpc, service, test::RpcSpawner{grpc_context, std::move(server_function), allocator});
        asio::spawn(get_executor(), std::move(client_function));
    }
};

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "yield_context repeatedly_request unary")
{
    auto request_received_count{0};
    auto request_send_count{0};
    std::vector<size_t> completion_order;
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        test::RpcSpawner{grpc_context,
                         [&](grpc::ServerContext&, test::msg::Request& request,
                             grpc::ServerAsyncResponseWriter<test::msg::Response>& writer, asio::yield_context yield)
                         {
                             ++request_received_count;
                             grpc::Alarm alarm;
                             if (0 == request.integer())
                             {
                                 agrpc::wait(alarm, test::five_hundred_milliseconds_from_now(), yield);
                             }
                             if (1 == request.integer())
                             {
                                 agrpc::wait(alarm, test::two_hundred_milliseconds_from_now(), yield);
                             }
                             test::msg::Response response;
                             response.set_integer(21);
                             CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
                         },
                         get_allocator()});
    for (size_t i = 0; i < 3; ++i)
    {
        asio::spawn(grpc_context,
                    [&](asio::yield_context yield)
                    {
                        test::PerformUnarySuccessOptions options;
                        options.request_payload = request_send_count;
                        ++request_send_count;
                        test::client_perform_unary_success(grpc_context, *stub, yield, options);
                        completion_order.emplace_back(options.request_payload);
                        if (3 == completion_order.size())
                        {
                            grpc_context.stop();
                        }
                    });
    }
    grpc_context.run();
    CHECK_EQ(3, request_received_count);
    CHECK(allocator_has_been_used());
    REQUIRE_EQ(3, completion_order.size());
    CHECK_EQ(2, completion_order[0]);
    CHECK_EQ(1, completion_order[1]);
    CHECK_EQ(0, completion_order[2]);
}

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "yield_context repeatedly_request client streaming")
{
    bool is_shutdown{false};
    auto request_count{0};
    std::optional<std::thread> server_shutdown_thread;
    this->test(
        &test::v1::Test::AsyncService::RequestClientStreaming, service,
        [&](grpc::ServerContext&, grpc::ServerAsyncReader<test::msg::Response, test::msg::Request>& reader,
            asio::yield_context yield)
        {
            CHECK(agrpc::send_initial_metadata(reader, yield));
            test::msg::Request request;
            CHECK(agrpc::read(reader, request, yield));
            CHECK_EQ(42, request.integer());
            ++request_count;
            if (request_count > 3)
            {
                is_shutdown = true;
            }
            test::msg::Response response;
            response.set_integer(21);
            CHECK(agrpc::finish(reader, response, grpc::Status::OK, yield));
        },
        [&](asio::yield_context yield)
        {
            while (!is_shutdown)
            {
                test::client_perform_client_streaming_success(*stub, yield);
            }
            server_shutdown_thread.emplace(
                [&]
                {
                    server->Shutdown();
                });
        },
        get_allocator());
    grpc_context.run();
    server_shutdown_thread->join();
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "RepeatedlyRequestContext member functions for multi-arg requests")
{
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        asio::bind_executor(
            get_executor(),
            [&](auto&& rpc_context)
            {
                auto&& request = rpc_context.request();
                CHECK(std::is_same_v<test::msg::Request&, decltype(request)>);
                auto&& responder = rpc_context.responder();
                CHECK(std::is_same_v<grpc::ServerAsyncResponseWriter<test::msg::Response>&, decltype(responder)>);
                auto&& context = rpc_context.server_context();
                CHECK(std::is_same_v<grpc::ServerContext&, decltype(context)>);
                test::msg::Response response;
                agrpc::finish(responder, response, grpc::Status::OK,
                              asio::bind_executor(get_executor(), [c = std::move(rpc_context)](bool) {}));
            }));
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    CHECK(test::client_perform_unary_unchecked(grpc_context, *stub, yield));
                    grpc_context.stop();
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "RepeatedlyRequestContext member functions for single-arg requests")
{
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestClientStreaming, service,
        asio::bind_executor(get_executor(),
                            [&](auto&& rpc_context)
                            {
                                auto&& responder = rpc_context.responder();
                                CHECK(std::is_same_v<grpc::ServerAsyncReader<test::msg::Response, test::msg::Request>&,
                                                     decltype(responder)>);
                                auto&& context = rpc_context.server_context();
                                CHECK(std::is_same_v<grpc::ServerContext&, decltype(context)>);
                                test::msg::Response response;
                                agrpc::finish(
                                    responder, response, grpc::Status::OK,
                                    asio::bind_executor(get_executor(), [c = std::move(rpc_context)](bool) {}));
                            }));
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::msg::Response response;
                    auto [writer, ok] = agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub,
                                                       client_context, response, yield);
                    agrpc::writes_done(*writer, yield);
                    grpc::Status status;
                    agrpc::finish(*writer, status, yield);
                    grpc_context.stop();
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "repeatedly_request tracks work of completion_handler's executor")
{
    int order{};
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
    asio::io_context io_context;
    agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestUnary, service,
                              asio::bind_executor(grpc_context, [&](auto&&) {}),
                              asio::bind_executor(asio::any_io_executor(io_context.get_executor()),
                                                  [&]
                                                  {
                                                      actual_thread_id = std::this_thread::get_id();
                                                      ++order;
                                                  }));
    std::thread io_context_thread{[&]
                                  {
                                      expected_thread_id = std::this_thread::get_id();
                                      io_context.run();
                                      order = 1 == order ? 2 : 0;
                                  }};
    std::optional<std::thread> server_shutdown_thread;
    asio::post(grpc_context,
               [&]
               {
                   server_shutdown_thread.emplace(
                       [&]
                       {
                           server->Shutdown();
                       });
               });
    grpc_context.run();
    io_context_thread.join();
    server_shutdown_thread->join();
    CHECK_EQ(2, order);
    CHECK_EQ(expected_thread_id, actual_thread_id);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "repeatedly_request cancellation")
{
    int count{};
    asio::cancellation_signal signal;
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        test::RpcSpawner{grpc_context,
                         [&](grpc::ServerContext&, test::msg::Request&,
                             grpc::ServerAsyncResponseWriter<test::msg::Response>& writer, asio::yield_context yield)
                         {
                             test::msg::Response response;
                             CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
                             ++count;
                         }},
        asio::bind_cancellation_slot(signal.slot(), test::NoOp{}));
    asio::spawn(grpc_context,
                [&](auto&& yield)
                {
                    signal.emit(asio::cancellation_type::all);
                    CHECK(test::client_perform_unary_unchecked(grpc_context, *stub, yield));
                });
    grpc_context.run();
    CHECK_EQ(1, count);
}
#endif
}