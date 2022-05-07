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

#include "utils/asioUtils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpcContextTest.hpp"
#include "utils/time.hpp"

#include <agrpc/bindAllocator.hpp>
#include <agrpc/wait.hpp>

#include <vector>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE(
    "AllocatorBinder can be constructed using allocator_traits<polymorphic_allocator>::construct with expected "
    "arguments")
{
    using PmrAllocator = agrpc::detail::pmr::polymorphic_allocator<std::byte>;
    using Binder = agrpc::AllocatorBinder<int, PmrAllocator>;
    agrpc::detail::pmr::monotonic_buffer_resource resource;
    PmrAllocator expected_allocator{&resource};
    std::vector<Binder, agrpc::detail::pmr::polymorphic_allocator<Binder>> vector;
    vector.emplace_back(expected_allocator);
    CHECK_EQ(expected_allocator, asio::get_associated_allocator(vector.front()));
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_allocator with awaitable")
{
    test::co_spawn(get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::Alarm alarm;
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(),
                                            agrpc::bind_allocator(get_allocator(), asio::use_awaitable));
                   });
    grpc_context.run();
    CHECK(allocator_has_been_used());
}
#endif
}