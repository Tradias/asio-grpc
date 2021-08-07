#ifndef AGRPC_DETAIL_INITIATE_HPP
#define AGRPC_DETAIL_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc::detail
{
template <class CompletionHandler, class Function>
auto create_work_and_invoke(CompletionHandler completion_handler, Function&& function)
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(completion_handler);
    // TODO C++17
    // auto&& grpc_context = static_cast<agrpc::GrpcContext&>(asio::query(executor, asio::execution::context_t{}));
    auto&& grpc_context = static_cast<agrpc::GrpcContext&>(executor.query(asio::execution::context_t{}));
    detail::create_work<true>(
        grpc_context, std::move(completion_handler),
        [&](auto* work)
        {
            function(grpc_context, work);
        },
        allocator);
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_INITIATE_HPP
