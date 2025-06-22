// Copyright 2025 Dennis Hezel
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

#include "utils/grpc_context_test.hpp"

#include "utils/asio_forward.hpp"

#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

namespace test
{
GrpcContextTest::GrpcContextTest()
    : resource{}, grpc_context_lifetime{builder.AddCompletionQueue()}, grpc_context{*grpc_context_lifetime}
{
}

agrpc::GrpcExecutor GrpcContextTest::get_executor() noexcept { return grpc_context.get_executor(); }

test::TrackingAllocator<> GrpcContextTest::get_allocator() noexcept { return test::TrackingAllocator<>(resource); }

bool GrpcContextTest::allocator_has_been_used() const noexcept { return resource.bytes_allocated > 0; }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
GrpcContextTrackingAllocatorExecutor GrpcContextTest::get_tracking_allocator_executor() noexcept
{
    return test::tracking_allocator_executor(grpc_context, get_allocator());
}

GrpcContextWorkTrackingExecutor GrpcContextTest::get_work_tracking_executor() noexcept
{
    return test::work_tracking_executor(grpc_context);
}

void GrpcContextTest::post(const std::function<void()>& function) { test::post(grpc_context, function); }
#endif
}  // namespace test
