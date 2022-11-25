# Difference to Asio

Asio-grpc strives to adhere to asio's asynchronous model as closely as possible. However, some parts of asio-grpc's older API deviate from that model. The exact differences are described below.

The free functions in this library (except those that take a GrpcContext/GrpcExecutor as their first argument) require that the completion handler's executor be created from a `agrpc::GrpcExecutor` and they use that as the I/O executor. Unlike Asio, where the I/O executor is obtained from the first argument to the initiating function. Example:

```cpp
asio::steady_timer timer{exec1}; // exec1 is the I/O executor
timer.async_wait(asio::bind_executor(exec2, [](auto&&) {})); // exec2 is the completion handler executor
```

In asio-grpc:

```cpp
grpc::Alarm alarm;
agrpc::wait(alarm, deadline, asio::bind_executor(grpc_context.get_executor(), [](auto&&) {}));
// grpc_context.get_executor() is both, the I/O executor and completion handler executor
```

As a consequence, the asynchronous operations always complete in the thread that called `GrpcContext::run*()/poll*()`, whereas Asio would submit the completion handler for execution as if by performing `asio::dispatch(exec2, [=<moved>]() { completion_handler(args...); })`. See also [Asio's documentation](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/asynchronous_operations.html).

More recently added APIs like the `agrpc::Alarm` behave exactly like Asio:

```cpp
agrpc::Alarm alarm{grpc_context} // grpc_context.get_executor() is the I/O executor
alarm.wait(100ms, asio::bind_executor(exec2, [](auto&&) {})); // exec2 is the completion handler executor
```
