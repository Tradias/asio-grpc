#ifndef AGRPC_DETAIL_GRPCCOMPLETIONQUEUEEVENT_HPP
#define AGRPC_DETAIL_GRPCCOMPLETIONQUEUEEVENT_HPP

namespace agrpc::detail
{
struct GrpcCompletionQueueEvent
{
    void* tag{nullptr};
    bool ok{false};
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCOMPLETIONQUEUEEVENT_HPP
