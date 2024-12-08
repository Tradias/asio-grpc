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

#ifndef AGRPC_UTILS_REQUESTMESSAGEFACTORY_HPP
#define AGRPC_UTILS_REQUESTMESSAGEFACTORY_HPP

#include <doctest/doctest.h>
#include <google/protobuf/arena.h>

namespace test
{
struct ArenaRequestMessageFactory
{
    google::protobuf::Arena arena;
    bool is_destroy_invoked{};

    ~ArenaRequestMessageFactory() { CHECK(is_destroy_invoked); }

    template <class Request>
    Request& create()
    {
        return *google::protobuf::Arena::Create<Request>(&arena);
    }

    template <class Request>
    void destroy(Request&) noexcept
    {
        is_destroy_invoked = true;
    }
};

template <class Handler>
struct RPCHandlerWithRequestMessageFactory
{
    explicit RPCHandlerWithRequestMessageFactory(Handler handler) : handler_(std::move(handler)) {}

    template <class... Args>
    decltype(auto) operator()(Args&&... args)
    {
        return handler_(static_cast<Args&&>(args)...);
    }

    ArenaRequestMessageFactory request_message_factory() { return {}; }

    Handler handler_;
};
}

#endif  // AGRPC_UTILS_REQUESTMESSAGEFACTORY_HPP
