#ifndef AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP
#define AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP

#include "protos/test.grpc.pb.h"
#include "utils/grpcContextTest.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace agrpc::test
{
struct GrpcClientServerTest : test::GrpcContextTest
{
    uint16_t port;
    std::string address;
    test::v1::Test::AsyncService service;
    std::unique_ptr<test::v1::Test::Stub> stub;
    grpc::ServerContext server_context;
    grpc::ClientContext client_context;

    GrpcClientServerTest();

    ~GrpcClientServerTest();
};
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP
