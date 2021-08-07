#ifndef AGRPC_UTILS_GRPCTEST_HPP
#define AGRPC_UTILS_GRPCTEST_HPP

#include "agrpc/grpcContext.hpp"
#include "agrpc/grpcExecutor.hpp"

#include <boost/asio.hpp>
#include <grpcpp/grpcpp.h>

#include <array>
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

    auto get_pmr_executor()
    {
        return grpc_context.get_executor().require(
            boost::asio::execution::allocator(std::pmr::polymorphic_allocator<std::byte>(&resource)));
        // TODO C++17
        // return boost::asio::require(
        //     grpc_context.get_executor(),
        //     boost::asio::execution::allocator(std::pmr::polymorphic_allocator<std::byte>(&resource)));
    }
};

inline auto ten_milliseconds_from_now() { return std::chrono::system_clock::now() + std::chrono::milliseconds(10); }
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_GRPCTEST_HPP
