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
#include "utils/test_server.hpp"
#include "utils/tracking_allocator.hpp"

#include <agrpc/async_notify_when_done.hpp>
#include <agrpc/bind_allocator.hpp>
#include <agrpc/rpc.hpp>

TEST_CASE("async_notify_when_done: deallocates unstarted operation on destruction")
{
    bool ok{false};
    test::TrackedAllocation tracked{};
    {
        test::GrpcContextTest test;
        grpc::ServerContext server_context;
        agrpc::async_notify_when_done(test.grpc_context, server_context,
                                      agrpc::bind_allocator(test::TrackingAllocator<std::byte>{tracked},
                                                            [&](bool done)
                                                            {
                                                                ok = done;
                                                            }));
        test.grpc_context.run();
    }
    CHECK_FALSE(ok);
    CHECK_LT(0, tracked.bytes_allocated);
    CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
}

TEST_CASE("async_notify_when_done: is completed on RPC success")
{
    bool ok{true};
    test::TrackedAllocation tracked{};
    test::TrackedAllocation tracked2{};
    {
        test::GrpcClientServerTest test;
        test::TestServer<&test::v1::Test::AsyncService::RequestUnary> test_server{test.service, test.server_context};
        test::spawn_and_run(
            test.grpc_context,
            [&](const asio::yield_context& yield)
            {
                agrpc::async_notify_when_done(test.grpc_context, test.server_context,
                                              agrpc::bind_allocator(test::TrackingAllocator<std::byte>{tracked},
                                                                    [&](bool)
                                                                    {
                                                                        ok = test.server_context.IsCancelled();
                                                                    }));
                CHECK(test_server.request_rpc(yield));
                test_server.response.set_integer(21);
                CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
            },
            [&](const asio::yield_context& yield)
            {
                test::client_perform_unary_success(test.grpc_context, *test.stub, yield);
            });
        tracked2 = tracked;
        CHECK_LT(0, tracked.bytes_allocated);
        CHECK_EQ(tracked.bytes_allocated, tracked.bytes_deallocated);
    }
    CHECK_FALSE(ok);
    CHECK_EQ(tracked2.bytes_allocated, tracked.bytes_allocated);
    CHECK_EQ(tracked2.bytes_deallocated, tracked.bytes_deallocated);
}