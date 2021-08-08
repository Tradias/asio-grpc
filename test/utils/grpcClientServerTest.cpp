#include "utils/grpcClientServerTest.hpp"

#include "protos/test.grpc.pb.h"
#include "utils/freePort.hpp"
#include "utils/grpcContextTest.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/server_context.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace agrpc::test
{
GrpcClientServerTest::GrpcClientServerTest()
    : port(agrpc::test::get_free_port()), address(std::string{"0.0.0.0:"} + std::to_string(port))
{
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    stub = test::v1::Test::NewStub(
        grpc::CreateChannel(std::string{"localhost:"} + std::to_string(port), grpc::InsecureChannelCredentials()));
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
}

GrpcClientServerTest::~GrpcClientServerTest()
{
    stub.reset();
    server->Shutdown();
}
}  // namespace agrpc::test
