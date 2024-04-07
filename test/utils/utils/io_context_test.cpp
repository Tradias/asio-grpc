// Copyright 2024 Dennis Hezel
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

#include "utils/io_context_test.hpp"

#include "utils/asio_forward.hpp"

#include <thread>

namespace test
{
IoContextTest::~IoContextTest()
{
    io_context_guard.reset();
    if (io_context_run_thread.joinable())
    {
        io_context_run_thread.join();
    }
}

void IoContextTest::run_io_context_detached(bool use_work_guard)
{
    if (use_work_guard)
    {
        io_context_guard.emplace(
            asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked));
    }
    io_context_run_thread = std::thread{[&]
                                        {
                                            io_context.run();
                                        }};
}
}  // namespace test
