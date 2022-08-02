# RPC cheat sheet

@tableofcontents

The code below is based on [example.proto](https://github.com/Tradias/asio-grpc/blob/d4bdcc0a06389127bb649ae4ea68185b928a5264/example/proto/example/v1/example.proto).

@note The last argument to all functions below is a completion token. The default is `asio::use_awaitable` which has been omitted.

@attention The completion handler created from the completion token that is provided to the functions described below
must have an associated executor that refers to a GrpcContext:
@snippet server.cpp bind-executor-to-use-awaitable

## Client

All RPC types support retrieving initial metadata immediately. This is optional step. Completes with `false` if the call is dead.

@snippet client.cpp read_initial_metadata-unary-client-side

### Unary

@snippet rpc_cheat_sheet.cpp full-unary-client-side

### Client-streaming

@snippet rpc_cheat_sheet.cpp full-client-streaming-client-side

### Server-streaming

@snippet rpc_cheat_sheet.cpp full-server-streaming-client-side

### Bidirectional-streaming

@snippet rpc_cheat_sheet.cpp full-bidirectional-client-side

## Server

All RPC types support sending initial metadata immediately. This is optional step. Completes with `false` if the call is dead.

@snippet server.cpp send_initial_metadata-unary-server-side

### Unary

To handle all requests for the specified method:

@snippet rpc_cheat_sheet.cpp full-unary-server-side

To handle just one request, e.g. to write your own accept loop. `request_ok` is `false` if the server is shutting down.

@snippet server.cpp request-unary-server-side

### Client-streaming

To handle all requests for the specified method:

@snippet rpc_cheat_sheet.cpp full-client-streaming-server-side

To handle just one request, e.g. to write your own accept loop. `request_ok` is `false` if the server is shutting down.

@snippet server.cpp request-client-streaming-server-side

### Server-streaming

To handle all requests for the specified method:

@snippet rpc_cheat_sheet.cpp full-server-streaming-server-side

To handle just one request, e.g. to write your own accept loop. `request_ok` is `false` if the server is shutting down.

@snippet server.cpp request-server-streaming-server-side

### Bidirectional-streaming

To handle all requests for the specified method:

@snippet rpc_cheat_sheet.cpp full-bidirectional-streaming-server-side

To handle just one request, e.g. to write your own accept loop. `request_ok` is `false` if the server is shutting down.

@snippet server.cpp request-bidirectional-streaming-server-side
