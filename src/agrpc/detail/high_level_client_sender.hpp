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

#ifndef AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
#define AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Responder>
struct ReadInitiateMetadataSenderImplementation
{
    void initiate(void* self) noexcept { detail::ReadInitialMetadataInitFunction{responder}(grpc_context, self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok) noexcept
    {
        if (is_finished)
        {
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(ok);
            return;
        }
        is_finished = true;
        detail::FinishInitFunction<Responder>{responder, status}(grpc_context, on_done.self());
    }

    agrpc::GrpcContext& grpc_context;
    Responder& responder;
    grpc::Status& status;
    bool is_finished{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
