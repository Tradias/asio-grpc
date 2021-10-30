// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPCCOMPLETIONQUEUEEVENT_HPP
#define AGRPC_DETAIL_GRPCCOMPLETIONQUEUEEVENT_HPP

namespace agrpc::detail
{
struct GrpcCompletionQueueEvent
{
    void* tag;
    bool ok;
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCOMPLETIONQUEUEEVENT_HPP
