// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_UTILS_CLIENT_CONTEXT_HPP
#define AGRPC_UTILS_CLIENT_CONTEXT_HPP

#include "utils/time.hpp"

#include <grpcpp/client_context.h>

#include <chrono>
#include <memory>

namespace test
{
void set_default_deadline(grpc::ClientContext& client_context);

std::unique_ptr<grpc::ClientContext> create_client_context(
    std::chrono::system_clock::time_point deadline = test::five_seconds_from_now());
}

#endif  // AGRPC_UTILS_CLIENT_CONTEXT_HPP
