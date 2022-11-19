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
#include "utils/rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/test_server.hpp"
#include "utils/tracking_allocator.hpp"

#include <agrpc/bind_allocator.hpp>
#include <agrpc/notify_when_done.hpp>
#include <agrpc/rpc.hpp>

struct NotifyWhenDoneTest
{
    std::optional<test::TestServer<&test::v1::Test::AsyncService::RequestUnary>> test_server;
    agrpc::GrpcContext client_grpc_context{std::make_unique<grpc::CompletionQueue>()};
    test::GrpcClientServerTest test;
    test::ServerShutdownInitiator server_shutdown{*test.server};

    NotifyWhenDoneTest() { test_server.emplace(test.service, test.server_context); }

    auto& grpc_context() { return test.grpc_context; }

    template <class CompletionToken>
    auto bind_grpc_context(const CompletionToken& token)
    {
        return asio::bind_executor(grpc_context(), token);
    }
};

template <class Function>
auto track_allocation(test::TrackedAllocation& tracked, Function function)
{
    return agrpc::bind_allocator(test::TrackingAllocator<std::byte>{tracked}, function);
}

TEST_CASE("notify_when_done: manually discount work")
{
    bool invoked{false};
    bool ok{true};
    test::TrackedAllocation tracked{};
    {
        NotifyWhenDoneTest test;
        agrpc::notify_when_done(test.grpc_context(), test.test.server_context,
                                track_allocation(tracked,
                                                 [&]
                                                 {
                                                     invoked = true;
                                                 }));
        test.test_server->request_rpc(test.bind_grpc_context(
            [&](bool request_ok)
            {
                ok = request_ok;
                if (request_ok)
                {
                    test.grpc_context().work_started();
                }
            }));
        test.grpc_context().work_finished();
        test::post(test.grpc_context(),
                   [&]
                   {
                       test.server_shutdown.initiate();
                   });
        test.grpc_context().run();
    }
    CHECK_FALSE(invoked);
    CHECK_FALSE(ok);
    CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
}

TEST_CASE("notify_when_done: deallocates unstarted operation on destruction")
{
    bool invoked{false};
    test::TrackedAllocation tracked{};
    {
        NotifyWhenDoneTest test;
        agrpc::notify_when_done(test.grpc_context(), test.test.server_context,
                                track_allocation(tracked,
                                                 [&]
                                                 {
                                                     invoked = true;
                                                 }));
        test.test_server->request_rpc(test.bind_grpc_context(
            [&](bool)
            {
                invoked = true;
            }));
        test.grpc_context().poll();
    }
    CHECK_FALSE(invoked);
    CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE("notify_when_done: is completed on RPC success")
{
    bool ok{true};
    test::TrackedAllocation tracked{};
    test::TrackedAllocation tracked2{};
    {
        NotifyWhenDoneTest test;
        agrpc::GrpcContext* grpc_context;
        SUBCASE("initiate from GrpcContext thread") { grpc_context = &test.grpc_context(); }
        SUBCASE("initiate from remote thread") { grpc_context = &test.client_grpc_context; }
        test.grpc_context().work_started();
        std::thread t{[&]
                      {
                          if (grpc_context == &test.client_grpc_context)
                          {
                              test.grpc_context().run();
                          }
                      }};
        test::spawn_and_run(
            *grpc_context,
            [&](const asio::yield_context& yield)
            {
                agrpc::notify_when_done(test.grpc_context(), test.test.server_context,
                                        track_allocation(tracked,
                                                         [&]
                                                         {
                                                             ok = test.test.server_context.IsCancelled();
                                                         }));
                CHECK(test.test_server->request_rpc(test.bind_grpc_context(yield)));
                test.test_server->response.set_integer(21);
                CHECK(agrpc::finish(test.test_server->responder, test.test_server->response, grpc::Status::OK,
                                    test.bind_grpc_context(yield)));
            },
            [&](const asio::yield_context& yield)
            {
                test::client_perform_unary_success(*grpc_context, *test.test.stub, yield);
                test.grpc_context().work_finished();
            });
        t.join();
        tracked2 = tracked;
        CHECK_LT(0, tracked.bytes_allocated);
        CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
    }
    CHECK_FALSE(ok);
    CHECK_EQ(tracked2.bytes_allocated, tracked.bytes_allocated);
    CHECK_EQ(tracked2.bytes_deallocated, tracked.bytes_deallocated);
}
#endif