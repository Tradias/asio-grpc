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
#include "utils/destruction_tracker.hpp"
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
    return agrpc::bind_allocator(test::TrackingAllocator{tracked}, std::move(function));
}

TEST_CASE("notify_when_done: manually discount work")
{
    bool invoked{false};
    bool ok{true};
    test::TrackedAllocation tracked;
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
                if (!request_ok)
                {
                    test.grpc_context().work_finished();
                }
            }));
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

TEST_CASE("notify_when_done: destructs and deallocates unstarted, remote operation on GrpcContext destruction")
{
    bool destructed{false};
    bool invoked{false};
    test::TrackedAllocation tracked;
    {
        NotifyWhenDoneTest test;
        agrpc::notify_when_done(test.grpc_context(), test.test.server_context,
                                track_allocation(tracked,
                                                 [&, d = test::DestructionTracker::make(destructed)]
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
    CHECK(destructed);
    CHECK_FALSE(invoked);
    CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
}

TEST_CASE("notify_when_done: destructs and deallocates unstarted, local operation on GrpcContext destruction")
{
    bool destructed{false};
    bool invoked{false};
    {
        NotifyWhenDoneTest test;
        test::spawn_and_run(test.grpc_context(),
                            [&](const asio::yield_context& yield)
                            {
                                agrpc::notify_when_done(test.grpc_context(), test.test.server_context,
                                                        [&, d = test::DestructionTracker::make(destructed)]
                                                        {
                                                            invoked = true;
                                                        });
                                asio::post(test.grpc_context(),
                                           [&]
                                           {
                                               test.grpc_context().stop();
                                           });
                                test.test_server->request_rpc(yield);
                                invoked = true;
                            });
    }
    CHECK(destructed);
    CHECK_FALSE(invoked);
}

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
TEST_CASE("notify_when_done: deallocates sender operation states only when necessary")
{
    bool use_submit{false};
    bool invoked{false};
    test::TrackedAllocation tracked;
    const auto receiver_function = [&]
    {
        invoked = true;
    };
    SUBCASE("connect") {}
    SUBCASE("submit") { use_submit = true; }
    {
        NotifyWhenDoneTest test;
        auto notify_when_done_sender =
            agrpc::notify_when_done(test.grpc_context(), test.test.server_context, agrpc::use_sender);
        const auto connect = [&]
        {
            return asio::execution::connect(notify_when_done_sender, test::FunctionAsReceiver{receiver_function});
        };
        test::spawn_and_run(test.grpc_context(),
                            [&](const asio::yield_context& yield)
                            {
                                std::optional<decltype(connect())> when_done_operation_state;
                                if (use_submit)
                                {
                                    asio::execution::submit(
                                        notify_when_done_sender,
                                        test::FunctionAsReceiver{receiver_function, test::TrackingAllocator{tracked}});
                                }
                                else
                                {
                                    when_done_operation_state.emplace(connect());
                                    asio::execution::start(*when_done_operation_state);
                                }
                                test::post(test.grpc_context(),
                                           [&]
                                           {
                                               test.server_shutdown.initiate();
                                           });
                                if (!test.test_server->request_rpc(yield))
                                {
                                    test.grpc_context().work_finished();
                                }
                            });
    }
    CHECK_FALSE(invoked);
    CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
}
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE("notify_when_done: is completed on RPC success")
{
    bool ok{true};
    test::TrackedAllocation tracked;
    test::TrackedAllocation tracked2;
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