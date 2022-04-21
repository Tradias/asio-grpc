# Difference to Asio

The free functions in this library (except `agrpc::repeatedly_request`) require that the completion handler executor be created from a `agrpc::GrpcExecutor` and they use that as the I/O executor. Unlike Asio, where the I/O executor is obtained from the first argument to the initiating function. Example:

```cpp
asio::steady_timer timer{exec1}; // exec1 is the I/O executor
timer.async_wait(asio::bind_executor(exec2, [](auto&&) {})); // exec2 is the completion handler executor
```

In asio-grpc:

```cpp
grpc::Alarm alarm;
agrpc::wait(alarm, deadline, asio::bind_executor(grpc_context.get_executor(), [](auto&&) {})); // grpc_context.get_executor() is both, the I/O executor and completion handler executor
```

As a consequence, asynchronous operations in asio-grpc always complete in the thread that called `GrpcContext::run()/poll()`, whereas Asio would submit the completion handler for execution as if by performing `asio::dispatch(exec2, [completion_handler](auto... args) { completion_handler(args...); })`. See also [asio's documentation](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/asynchronous_operations.html).   
Please open a ticket if you would like to have a wrapper class in asio-grpc that mimics that behavior, it is rather easy to implement.