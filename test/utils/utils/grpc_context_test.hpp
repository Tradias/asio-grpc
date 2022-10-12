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
#endif

struct GrpcContextTest
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    std::array<std::byte, 4096> buffer;
    agrpc::detail::pmr::monotonic_buffer_resource resource;
    std::optional<agrpc::GrpcContext> grpc_context_lifetime;
    agrpc::GrpcContext& grpc_context;

    GrpcContextTest();

    agrpc::GrpcExecutor get_executor() noexcept;

    agrpc::detail::pmr::polymorphic_allocator<std::byte> get_allocator() noexcept;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    agrpc::pmr::GrpcExecutor get_pmr_executor() noexcept;

    auto get_work_tracking_executor() noexcept { return test::work_tracking_executor(grpc_context); }

    void wait(grpc::Alarm& alarm, std::chrono::system_clock::time_point deadline,
              const std::function<void(bool)>& callback)
    {
        test::wait(alarm, deadline, asio::bind_executor(grpc_context, callback));
    }

    void post(const std::function<void()>& function) { test::post(grpc_context, function); }
#endif

    auto use_sender() noexcept { return agrpc::use_sender(get_executor()); }

    bool allocator_has_been_used() noexcept;
};
}  // namespace test

#endif  // AGRPC_UTILS_GRPC_CONTEXT_TEST_HPP
