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

#ifndef AGRPC_DETAIL_TEST_HPP
#define AGRPC_DETAIL_TEST_HPP

#include <agrpc/detail/grpc_context_implementation.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct ProcessTag
{
    agrpc::GrpcContext& grpc_context_;
    void* tag_;
    bool ok_;

    template <class... T>
    void operator()(T&&...)
    {
        detail::process_grpc_tag(tag_, ok_ ? detail::OperationResult::OK_ : detail::OperationResult::NOT_OK,
                                 grpc_context_);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_TEST_HPP
