#ifndef AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP
#define AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP

#include "protos/test.grpc.pb.h"
#include "utils/grpcContextTest.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace agrpc::test
{
struct GrpcContextClientServerTest : test::GrpcContextTest
{
    uint16_t port;
    std::string address;
    test::v1::Test::AsyncService service;
    std::unique_ptr<test::v1::Test::Stub> stub;
    grpc::ServerContext server_context;
    grpc::ClientContext client_context;

    GrpcContextClientServerTest();

    ~GrpcContextClientServerTest();
};

}  // namespace agrpc::test

#endif  // AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP
