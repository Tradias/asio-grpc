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

#ifndef AGRPC_UTILS_SERVER_SHUTDOWN_INITIATOR_HPP
#define AGRPC_UTILS_SERVER_SHUTDOWN_INITIATOR_HPP

#include <grpcpp/server.h>

#include <thread>

namespace test
{
struct ServerShutdownInitiator
{
    grpc::Server& server;
    std::thread thread;

    ServerShutdownInitiator(grpc::Server& server);

    void initiate();

    ~ServerShutdownInitiator();
};
}  // namespace test

#endif  // AGRPC_UTILS_SERVER_SHUTDOWN_INITIATOR_HPP
