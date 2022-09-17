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
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/grpc_generic_client_server_test.hpp"
#include "utils/io_context_test.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"

#include <agrpc/repeatedly_request.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>

#include <cstddef>
#include <optional>
#include <thread>
#include <vector>

struct TypedRequestHandler
{
    using Test = test::GrpcClientServerTest;

    template <class RPC, class RequestHandler>
    static auto invoke_repeatedly_request(RPC rpc, test::v1::Test::AsyncService& service, RequestHandler handler)
    {
        return agrpc::repeatedly_request(rpc, service, handler);
    }

    static auto get_request_args(grpc::ServerContext&, test::msg::Request& request,
                                 grpc::ServerAsyncResponseWriter<test::msg::Response>& writer,
                                 asio::yield_context yield)
    {
        return std::make_tuple(request, writer, std::move(yield));
    }

    static void write_response(grpc::ServerAsyncResponseWriter<test::msg::Response>& writer,
                               const test::msg::Response& response, const asio::yield_context& yield)
    {
        CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
    }
};

TYPE_TO_STRING(TypedRequestHandler);

struct GenericRequestHandler
{
    using Test = test::GrpcGenericClientServerTest;

    template <class RPC, class RequestHandler>
    static auto invoke_repeatedly_request(RPC, grpc::AsyncGenericService& service, RequestHandler handler)
    {
        return agrpc::repeatedly_request(service, handler);
    }

    static auto get_request_args(grpc::GenericServerContext&, grpc::GenericServerAsyncReaderWriter& reader_writer,
                                 asio::yield_context yield)
    {
        grpc::ByteBuffer buffer;
        CHECK(agrpc::read(reader_writer, buffer, yield));
        return std::make_tuple(test::grpc_buffer_to_message<test::msg::Request>(buffer), reader_writer,
                               std::move(yield));
    }

    static void write_response(grpc::GenericServerAsyncReaderWriter& reader_writer, const test::msg::Response& response,
                               const asio::yield_context& yield)
    {
        const auto response_buffer = test::message_to_grpc_buffer(response);
        CHECK(agrpc::write_and_finish(reader_writer, response_buffer, {}, grpc::Status::OK, yield));
    }
};

TYPE_TO_STRING(GenericRequestHandler);

TEST_CASE_TEMPLATE("yield_context repeatedly_request unary", T, TypedRequestHandler, GenericRequestHandler)
{
    typename T::Test test;
    auto request_received_count{0};
    auto request_send_count{0};
    std::vector<size_t> completion_order;
    T::invoke_repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, test.service,
        test::RpcSpawner{test.grpc_context,
                         [&](auto&&... args)
                         {
                             auto&& [request, writer, yield] =
                                 T::get_request_args(std::forward<decltype(args)>(args)...);
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
                             T::write_response(writer, response, yield);
                         },
                         test.get_allocator()});
    test::v1::Test::Stub test_stub{test.channel};
    for (size_t i = 0; i < 3; ++i)
    {
        asio::spawn(test.grpc_context,
                    [&](const asio::yield_context& yield)
                    {
                        test::PerformUnarySuccessOptions options;
                        options.request_payload = request_send_count;
                        ++request_send_count;
                        test::client_perform_unary_success(test.grpc_context, test_stub, yield, options);
                        completion_order.emplace_back(options.request_payload);
                        if (3 == completion_order.size())
                        {
                            test.grpc_context.stop();
                        }
                    });
    }
    test.grpc_context.run();
    CHECK_EQ(3, request_received_count);
    CHECK(test.allocator_has_been_used());
    REQUIRE_EQ(3, completion_order.size());
    CHECK_EQ(2, completion_order[0]);
    CHECK_EQ(1, completion_order[1]);
    CHECK_EQ(0, completion_order[2]);
}

struct GrpcRepeatedlyRequestTest : test::GrpcClientServerTest, test::IoContextTest
{
    template <class RPC, class Service, class ServerFunction, class ClientFunction, class Allocator>
    auto test(RPC rpc, Service& service, ServerFunction server_function, ClientFunction client_function,
              Allocator allocator)
    {
        agrpc::repeatedly_request(rpc, service, test::RpcSpawner{grpc_context, std::move(server_function), allocator});
        test::spawn_and_run(grpc_context, std::move(client_function));
    }
};

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "yield_context repeatedly_request client streaming")
{
    bool is_shutdown{false};
    auto request_count{0};
    test::ServerShutdownInitiator server_shutdown{*server};
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
        [&](const asio::yield_context& yield)
        {
            while (!is_shutdown)
            {
                test::client_perform_client_streaming_success(*stub, yield);
            }
            server_shutdown.initiate();
        },
        get_allocator());
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
    test::spawn_and_run(grpc_context,
                        [&](asio::yield_context yield)
                        {
                            CHECK(test::client_perform_unary_unchecked(grpc_context, *stub, yield));
                            grpc_context.stop();
                        });
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
    test::spawn_and_run(grpc_context,
                        [&](asio::yield_context yield)
                        {
                            test::msg::Response response;
                            auto [writer, ok] = agrpc::request(&test::v1::Test::Stub::PrepareAsyncClientStreaming,
                                                               *stub, client_context, response, yield);
                            agrpc::writes_done(*writer, yield);
                            grpc::Status status;
                            agrpc::finish(*writer, status, yield);
                            grpc_context.stop();
                        });
}

TEST_CASE_FIXTURE(test::GrpcGenericClientServerTest, "RepeatedlyRequestContext member functions for generic requests")
{
    agrpc::repeatedly_request(
        service,
        asio::bind_executor(get_executor(),
                            [&](agrpc::GenericRepeatedlyRequestContext<>&& rpc_context)
                            {
                                [[maybe_unused]] auto&& responder = rpc_context.responder();
                                CHECK(std::is_same_v<grpc::GenericServerAsyncReaderWriter&, decltype(responder)>);
                                [[maybe_unused]] auto&& context = rpc_context.server_context();
                                CHECK(std::is_same_v<grpc::GenericServerContext&, decltype(context)>);
                            }));
    test::v1::Test::Stub test_stub{channel};
    test::spawn_and_run(grpc_context,
                        [&](asio::yield_context yield)
                        {
                            CHECK(test::client_perform_unary_unchecked(grpc_context, test_stub, yield,
                                                                       test::ten_milliseconds_from_now()));
                            grpc_context.stop();
                        });
}

TEST_CASE_FIXTURE(GrpcRepeatedlyRequestTest, "repeatedly_request tracks work of completion_handler's executor")
{
    int order{};
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
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
    post(
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
    test::spawn_and_run(grpc_context,
                        [&](auto&& yield)
                        {
                            signal.emit(asio::cancellation_type::all);
                            CHECK(test::client_perform_unary_unchecked(grpc_context, *stub, yield));
                        });
    CHECK_EQ(1, count);
}
#endif