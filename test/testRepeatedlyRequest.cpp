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

#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <thread>

namespace test_asio_grpc
{
TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

struct GrpcRepeatedlyRequestTest : test::GrpcClientServerTest
{
    template <class RPC, class Service, class ServerFunction, class ClientFunction, class Allocator>
    auto test(RPC rpc, Service& service, ServerFunction server_function, ClientFunction client_function,
              Allocator allocator)
    {
        agrpc::repeatedly_request(rpc, service,
                                  test::RpcSpawner{this->get_executor(), std::move(server_function), allocator});
        asio::spawn(get_executor(), std::move(client_function));
    }
};

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "yield_context repeatedly_request unary")
{
    bool is_shutdown{false};
    auto request_count{0};
    this->test(
        &test::v1::Test::AsyncService::RequestUnary, service,
        [&](grpc::ServerContext&, test::v1::Request& request,
            grpc::ServerAsyncResponseWriter<test::v1::Response>& writer, asio::yield_context yield)
        {
            CHECK_EQ(42, request.integer());
            test::v1::Response response;
            response.set_integer(21);
            ++request_count;
            if (request_count > 3)
            {
                is_shutdown = true;
            }
            CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
        },
        [&](asio::yield_context yield)
        {
            while (!is_shutdown)
            {
                test::v1::Request request;
                request.set_integer(42);
                grpc::ClientContext new_client_context;
                auto reader =
                    stub->AsyncUnary(&new_client_context, request, agrpc::get_completion_queue(get_executor()));
                test::v1::Response response;
                grpc::Status status;
                CHECK(agrpc::finish(*reader, response, status, yield));
                CHECK(status.ok());
                CHECK_EQ(21, response.integer());
            }
            grpc_context.stop();
        },
        get_allocator());
    grpc_context.run();
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "yield_context repeatedly_request client streaming")
{
    bool is_shutdown{false};
    auto request_count{0};
    std::optional<std::thread> server_shutdown_thread;
    this->test(
        &test::v1::Test::AsyncService::RequestClientStreaming, service,
        [&](grpc::ServerContext&, grpc::ServerAsyncReader<test::v1::Response, test::v1::Request>& reader,
            asio::yield_context yield)
        {
            test::v1::Request request;
            CHECK(agrpc::read(reader, request, yield));
            CHECK_EQ(42, request.integer());
            test::v1::Response response;
            response.set_integer(21);
            ++request_count;
            if (request_count > 3)
            {
                is_shutdown = true;
            }
            CHECK(agrpc::finish(reader, response, grpc::Status::OK, yield));
        },
        [&](asio::yield_context yield)
        {
            while (!is_shutdown)
            {
                test::v1::Response response;
                grpc::ClientContext new_client_context;
                auto [writer, ok] = agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub,
                                                   new_client_context, response, yield);
                CHECK(ok);
                test::v1::Request request;
                request.set_integer(42);
                CHECK(agrpc::write(*writer, request, yield));
                CHECK(agrpc::writes_done(*writer, yield));
                grpc::Status status;
                CHECK(agrpc::finish(*writer, status, yield));
                CHECK(status.ok());
                CHECK_EQ(21, response.integer());
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

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "GrpcContext.stop() before repeatedly_request")
{
    bool done{};
    grpc_context.stop();
    agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestUnary, service,
                              asio::bind_executor(get_executor(), [&](auto&&) {}),
                              [&]
                              {
                                  done = true;
                              });
    grpc_context.run();
    CHECK(done);
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
                CHECK(std::is_same_v<test::v1::Request&, decltype(request)>);
                auto&& responder = rpc_context.responder();
                CHECK(std::is_same_v<grpc::ServerAsyncResponseWriter<test::v1::Response>&, decltype(responder)>);
                auto&& context = rpc_context.server_context();
                CHECK(std::is_same_v<grpc::ServerContext&, decltype(context)>);
                test::v1::Response response;
                agrpc::finish(responder, response, grpc::Status::OK,
                              asio::bind_executor(get_executor(), [c = std::move(rpc_context)](bool) {}));
            }));
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Request request;
                    auto reader =
                        stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(get_executor()));
                    test::v1::Response response;
                    grpc::Status status;
                    agrpc::finish(*reader, response, status, yield);
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
                                CHECK(std::is_same_v<grpc::ServerAsyncReader<test::v1::Response, test::v1::Request>&,
                                                     decltype(responder)>);
                                auto&& context = rpc_context.server_context();
                                CHECK(std::is_same_v<grpc::ServerContext&, decltype(context)>);
                                test::v1::Response response;
                                agrpc::finish(
                                    responder, response, grpc::Status::OK,
                                    asio::bind_executor(get_executor(), [c = std::move(rpc_context)](bool) {}));
                            }));
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Response response;
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
        test::RpcSpawner{get_executor(),
                         [&](grpc::ServerContext&, test::v1::Request&,
                             grpc::ServerAsyncResponseWriter<test::v1::Response>& writer, asio::yield_context yield)
                         {
                             test::v1::Response response;
                             CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
                             ++count;
                         }},
        asio::bind_cancellation_slot(signal.slot(), asio::detached));
    asio::spawn(grpc_context,
                [&](auto&& yield)
                {
                    signal.emit(asio::cancellation_type::all);
                    test::v1::Request request;
                    grpc::ClientContext new_client_context;
                    auto reader =
                        stub->AsyncUnary(&new_client_context, request, agrpc::get_completion_queue(get_executor()));
                    test::v1::Response response;
                    grpc::Status status;
                    CHECK(agrpc::finish(*reader, response, status, yield));
                });
    grpc_context.run();
    CHECK_EQ(1, count);
}
#endif

TEST_SUITE_END();
}  // namespace test_asio_grpc