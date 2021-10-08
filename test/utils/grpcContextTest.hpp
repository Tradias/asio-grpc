// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_UTILS_GRPCTEST_HPP
#define AGRPC_UTILS_GRPCTEST_HPP

#include "agrpc/asioGrpc.hpp"
#include "utils/memoryResource.hpp"

#include <boost/asio/execution/allocator.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>

namespace agrpc::test
{
struct GrpcContextTest
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    std::array<std::byte, 1024> buffer{};
    agrpc::detail::pmr::monotonic_buffer_resource resource{buffer.data(), buffer.size()};
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};

    agrpc::GrpcExecutor get_executor() noexcept { return grpc_context.get_executor(); }

    auto get_allocator() noexcept { return agrpc::detail::pmr::polymorphic_allocator<std::byte>(&resource); }

    agrpc::pmr::GrpcExecutor get_pmr_executor() noexcept
    {
        return this->get_executor().require(asio::execution::allocator(get_allocator()));
    }

    auto get_work_tracking_executor() noexcept
    {
        return asio::require(get_executor(), asio::execution::outstanding_work.tracked);
    }

    auto get_work_tracking_pmr_executor() noexcept
    {
        return asio::require(get_work_tracking_executor(), asio::execution::allocator(get_allocator()));
    }
};

inline auto ten_milliseconds_from_now() { return std::chrono::system_clock::now() + std::chrono::milliseconds(10); }
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_GRPCTEST_HPP
