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

#ifndef AGRPC_UTILS_GRPCTEST_HPP
#define AGRPC_UTILS_GRPCTEST_HPP

#include "agrpc/asioGrpc.hpp"
#include "utils/asioForward.hpp"
#include "utils/memoryResource.hpp"

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>

namespace test
{
struct GrpcContextTest
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    std::array<std::byte, 4096> buffer{};
    agrpc::detail::pmr::monotonic_buffer_resource resource{buffer.data(), buffer.size()};
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};

    agrpc::GrpcExecutor get_executor() noexcept;

    agrpc::detail::pmr::polymorphic_allocator<std::byte> get_allocator() noexcept;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    agrpc::pmr::GrpcExecutor get_pmr_executor() noexcept;
#endif

    auto use_sender() noexcept { return agrpc::use_sender(get_executor()); }

    bool allocator_has_been_used() noexcept;
};

std::chrono::system_clock::time_point ten_milliseconds_from_now();

std::chrono::system_clock::time_point hundred_milliseconds_from_now();

std::chrono::system_clock::time_point one_seconds_from_now();

std::chrono::system_clock::time_point five_seconds_from_now();
}  // namespace test

#endif  // AGRPC_UTILS_GRPCTEST_HPP
