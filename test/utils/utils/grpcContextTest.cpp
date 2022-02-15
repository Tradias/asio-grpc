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

#include "utils/grpcContextTest.hpp"

#include "utils/asioForward.hpp"
#include "utils/memoryResource.hpp"

#include <agrpc/grpcContext.hpp>
#include <agrpc/grpcExecutor.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

namespace test
{
agrpc::GrpcExecutor GrpcContextTest::get_executor() noexcept { return grpc_context.get_executor(); }

agrpc::detail::pmr::polymorphic_allocator<std::byte> GrpcContextTest::get_allocator() noexcept
{
    return agrpc::detail::pmr::polymorphic_allocator<std::byte>(&resource);
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
agrpc::pmr::GrpcExecutor GrpcContextTest::get_pmr_executor() noexcept
{
    return asio::require(this->get_executor(), asio::execution::allocator(get_allocator()));
}
#endif

bool GrpcContextTest::allocator_has_been_used() noexcept
{
    return std::any_of(buffer.begin(), buffer.end(),
                       [](auto&& value)
                       {
                           return value != std::byte{};
                       });
}
}  // namespace test
