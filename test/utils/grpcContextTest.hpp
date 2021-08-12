#ifndef AGRPC_UTILS_GRPCTEST_HPP
#define AGRPC_UTILS_GRPCTEST_HPP

#include "agrpc/asioGrpc.hpp"

#include <boost/asio/execution/allocator.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <array>
#include <chrono>
#include <memory>
#include <memory_resource>

namespace agrpc::test
{
struct GrpcContextTest
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    std::array<std::byte, 1024> buffer{};
    std::pmr::monotonic_buffer_resource resource{buffer.data(), buffer.size()};

    agrpc::GrpcExecutor get_executor() noexcept { return grpc_context.get_executor(); }

    agrpc::pmr::GrpcExecutor get_pmr_executor() noexcept
    {
        return this->get_executor().require(
            boost::asio::execution::allocator(std::pmr::polymorphic_allocator<std::byte>(&resource)));
    }

    auto get_work_tracking_executor() noexcept
    {
        return asio::require(get_executor(), asio::execution::outstanding_work.tracked);
    }
};

inline auto ten_milliseconds_from_now() { return std::chrono::system_clock::now() + std::chrono::milliseconds(10); }
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_GRPCTEST_HPP
