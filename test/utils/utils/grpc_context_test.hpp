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

#ifndef AGRPC_UTILS_GRPC_CONTEXT_TEST_HPP
#define AGRPC_UTILS_GRPC_CONTEXT_TEST_HPP

#include "utils/asio_forward.hpp"
#include "utils/asio_utils.hpp"
#include "utils/grpc_format.hpp"
#include "utils/memory_resource.hpp"
#include "utils/tracking_allocator.hpp"

#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/use_sender.hpp>
#include <agrpc/wait.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

namespace test
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
inline auto work_tracking_executor(agrpc::GrpcContext& grpc_context) noexcept
{
    return asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked);
}

using GrpcContextWorkTrackingExecutor = decltype(test::work_tracking_executor(std::declval<agrpc::GrpcContext&>()));

inline auto tracking_allocator_executor(agrpc::GrpcContext& grpc_context, test::TrackingAllocator<> allocator) noexcept
{
    return asio::require(grpc_context.get_executor(), asio::execution::allocator(allocator));
}

using GrpcContextTrackingAllocatorExecutor =
    decltype(test::tracking_allocator_executor(std::declval<agrpc::GrpcContext&>(), test::TrackingAllocator<>{}));
#endif

struct GrpcContextTest
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    test::TrackedAllocation resource;
    std::optional<agrpc::GrpcContext> grpc_context_lifetime;
    agrpc::GrpcContext& grpc_context;

    GrpcContextTest();

    agrpc::GrpcExecutor get_executor() noexcept;

    test::TrackingAllocator<> get_allocator() noexcept;

    auto use_sender() noexcept { return agrpc::use_sender(get_executor()); }

    bool allocator_has_been_used() noexcept;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    GrpcContextTrackingAllocatorExecutor get_tracking_allocator_executor() noexcept;

    GrpcContextWorkTrackingExecutor get_work_tracking_executor() noexcept;

    void wait(grpc::Alarm& alarm, std::chrono::system_clock::time_point deadline,
              const std::function<void(bool)>& callback);

    void post(const std::function<void()>& function);
#endif
};
}  // namespace test

#endif  // AGRPC_UTILS_GRPC_CONTEXT_TEST_HPP
