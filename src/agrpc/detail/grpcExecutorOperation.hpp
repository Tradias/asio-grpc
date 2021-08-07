#ifndef AGRPC_DETAIL_GRPCEXECUTOROPERATION_HPP
#define AGRPC_DETAIL_GRPCEXECUTOROPERATION_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/functional.hpp"
#include "agrpc/detail/grpcContextOperation.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/utility.hpp"

namespace agrpc::detail
{
template <class Handler, class Allocator>
struct GrpcExecutorOperation : detail::GrpcContextOperation
{
  public:
    detail::CompressedPair<Handler, Allocator> impl;

    template <class H>
    GrpcExecutorOperation(H&& handler, Allocator allocator)
        : detail::GrpcContextOperation(&GrpcExecutorOperation::do_complete),
          impl(std::forward<H>(handler), std::move(allocator))
    {
    }

    static void do_complete(detail::GrpcContextOperation* base, bool ok,
                            detail::GrpcContextOperation::InvokeHandler invoke_handler)
    {
        using Deleter = detail::AllocatorDeleter<Allocator>;
        auto* self = static_cast<GrpcExecutorOperation*>(base);
        std::unique_ptr<GrpcExecutorOperation, Deleter> ptr{self, Deleter{std::move(self->impl.second())}};
        if (invoke_handler == detail::GrpcContextOperation::InvokeHandler::YES)
        {
            // Make a copy of the handler so that the memory can be deallocated before the upcall is made.
            auto handler{std::move(self->handler())};
            ptr.reset();
            detail::invoke_front(handler, ok);
        }
    }

    [[nodiscard]] decltype(auto) handler() noexcept { return impl.first(); }

    [[nodiscard]] decltype(auto) handler() const noexcept { return impl.first(); }
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCEXECUTOROPERATION_HPP
