# asio-grpc

This library provides an implementation of Boost.Asio's [execution_context](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/execution_context.html) which dispatches work to a gRPC [CompletionQueue](https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html). Making it possible to write 
asynchronous gRPC servers and clients using C++20 coroutines, Boost.Coroutines, Boost.Asio's stackless coroutines, std::futures and callbacks.

# Usage

```cmake
find_package(asio-grpc)
target_link_libraries(your_library PUBLIC asio-grpc::asio-grpc)

# OR 

add_subdirectory(/path/to/repository/root)
target_link_libraries(your_library PUBLIC asio-grpc::asio-grpc)
```

To exactly one of your `.cpp` files add the include:

```c++
#include <agrpc/asioGrpcSrc.hpp>
```

When using the library in other files use:

```c++
#include <agrpc/asioGrpc.hpp>
```

# Example

Server side:

```c++
grpc::ServerBuilder builder;
std::unique_ptr<grpc::Server> server;
test::v1::Test::AsyncService service;
agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
builder.RegisterService(&service);
server = builder.BuildAndStart();

boost::asio::co_spawn(grpc_context, [&]() -> boost::asio::awaitable<void> {
    grpc::ServerContext server_context;
    test::v1::Request request;
    grpc::ServerAsyncResponseWriter<test::v1::Response> writer{&server_context};
    bool request_ok = co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request, writer);
    test::v1::Response response;
    bool finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
}, boost::asio::detached);

grpc_context.run();
```

Client side:

```c++
auto stub = test::v1::Test::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

boost::asio::co_spawn(grpc_context, [&]() -> boost::asio::awaitable<void> {
    test::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<test::v1::Response>> reader = 
        co_await agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context, request);
    test::v1::Response response;
    grpc::Status status;
    bool ok = co_await agrpc::finish(*reader, response, status);
}, boost::asio::detached);

grpc_context.run();
```