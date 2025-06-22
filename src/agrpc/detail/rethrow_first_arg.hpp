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

#ifndef AGRPC_DETAIL_RETHROW_FIRST_ARG_HPP
#define AGRPC_DETAIL_RETHROW_FIRST_ARG_HPP

#include <exception>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RethrowFirstArg
{
    template <class... Args>
    void operator()(const std::exception_ptr& ep, Args&&...) const
    {
        if AGRPC_UNLIKELY (ep)
        {
            std::rethrow_exception(ep);
        }
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RETHROW_FIRST_ARG_HPP
