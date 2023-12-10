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

#ifndef AGRPC_HELPER_RETHROW_FIRST_ARG_HPP
#define AGRPC_HELPER_RETHROW_FIRST_ARG_HPP

#include <exception>

namespace example
{
// Using this as the completion token to functions like asio::co_spawn ensures that exceptions thrown by the coroutine
// are rethrown from grpc_context.run().
struct RethrowFirstArg
{
    template <class... T>
    void operator()(std::exception_ptr ep, T&&...)
    {
        if (ep)
        {
            std::rethrow_exception(ep);
        }
    }

    template <class... T>
    void operator()(T&&...)
    {
    }
};
}  // namespace example

#endif  // AGRPC_HELPER_RETHROW_FIRST_ARG_HPP
