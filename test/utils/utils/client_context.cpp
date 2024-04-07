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

#include "utils/client_context.hpp"

#include "utils/time.hpp"

#include <grpcpp/client_context.h>

#include <chrono>
#include <memory>

namespace test
{
void set_default_deadline(grpc::ClientContext& client_context)
{
    client_context.set_deadline(test::five_seconds_from_now());
}

std::unique_ptr<grpc::ClientContext> create_client_context(std::chrono::system_clock::time_point deadline)
{
    auto client_context = std::make_unique<grpc::ClientContext>();
    client_context->set_deadline(deadline);
    return client_context;
}
}
