// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_HELPER_AWAITABLE_CLIENT_RPC_HPP
#define AGRPC_HELPER_AWAITABLE_CLIENT_RPC_HPP

#include <agrpc/client_rpc.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace example
{
template <auto PrepareAsync>
using AwaitableClientRPC = boost::asio::use_awaitable_t<>::as_default_on_t<agrpc::ClientRPC<PrepareAsync>>;
}

#endif  // AGRPC_HELPER_AWAITABLE_CLIENT_RPC_HPP
