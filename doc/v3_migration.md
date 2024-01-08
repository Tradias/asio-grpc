# V3 migration guide

The following headers have been removed:

`bind_allocator.hpp`, `cancel_safe.hpp`, `default_completion_token.hpp`, `get_completion_queue.hpp`, `grpc_initiate.hpp`, `grpc_stream.hpp`, `notify_when_done.hpp`, `repeatedly_request_context.hpp`, `repeatedly_request.hpp`, `rpc.hpp`, `use_awaitable.hpp`, `wait.hpp`

## CMake

`asio-grpc` targets now link with `gRPC::grpc++` instead of `gRPC::grpc++_unsecure`. To restore the old behavior use:

```cmake
set(ASIO_GRPC_DISABLE_AUTOLINK on)
find_package(asio-grpc)
find_package(gRPC)

target_link_libraries(your_app PUBLIC asio-grpc::asio-grpc gRPC::grpc++_unsecure)
```

`asio-grpcConfig.cmake` no longer finds and links backend libraries (like Boost.Asio and unifex) to their respective `asio-grpc::asio-grpc` targets. Example on how to restore the old behavior for the Boost.Asio backend:

```cmake
find_package(asio-grpc)
# New in v3:
find_package(Boost)

target_link_libraries(your_app PUBLIC
    asio-grpc::asio-grpc 
    # New in v3:
    Boost::headers
)
```

## Alarms

The free function `agrpc::wait` has been replaced with a new I/O-object like class called `agrpc::Alarm`:

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp agrpc-wait            |   @snippet alarm.cpp agrpc-alarm |

## Client RPCs

Migration of client rpc types based on [example.proto](https://github.com/Tradias/asio-grpc/blob/d4bdcc0a06389127bb649ae4ea68185b928a5264/example/proto/example/v1/example.proto):

### Unary

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-unary-client-side            |   @snippet client_rpc.cpp client-rpc-unary |

### Client-streaming

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-client-streaming-client-side            |   @snippet client_rpc.cpp client-rpc-client-streaming |

### Server-streaming

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-server-streaming-client-side            |   @snippet client_rpc.cpp client-rpc-server-streaming |

### Bidirectional-streaming

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-bidirectional-client-side            |   @snippet client_rpc.cpp client-rpc-bidirectional-streaming |

## Server RPCs

Migration of server rpc types based on [example.proto](https://github.com/Tradias/asio-grpc/blob/d4bdcc0a06389127bb649ae4ea68185b928a5264/example/proto/example/v1/example.proto):

### Unary

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-unary-server-side            |   @snippet server_rpc.cpp server-rpc-unary |

### Client-streaming

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-client-streaming-server-side            |   @snippet server_rpc.cpp server-rpc-client-streaming |

### Server-streaming

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-server-streaming-server-side            |   @snippet server_rpc.cpp server-rpc-server-streaming |

### Bidirectional-streaming

| V2                          |   V3    |
|-----------------------------|---------|
| @snippet rpc_cheat_sheet.cpp full-bidirectional-streaming-server-side            |   @snippet server_rpc.cpp server-rpc-bidirectional-streaming |