# RPC cheat sheet

@tableofcontents

The code below is based on the [example.proto](https://github.com/Tradias/asio-grpc/blob/d4bdcc0a06389127bb649ae4ea68185b928a5264/example/proto/example/v1/example.proto) file.

@note The last argument to all functions below is a completion token. The default is `asio::use_awaitable` which has been omitted.

@attention The completion handler created from the completion token that is provided to the functions described below
must have an associated executor that refers to a GrpcContext:
@snippet server.cpp bind-executor-to-use-awaitable

## Client-side unary

@snippet rpc_cheat_sheet.cpp full-unary-client-side

## Client-side client-streaming

@snippet rpc_cheat_sheet.cpp full-client-streaming-client-side

## Client-side server-streaming

@snippet rpc_cheat_sheet.cpp full-server-streaming-client-side

## Client-side bidirectional-streaming

@snippet rpc_cheat_sheet.cpp full-bidirectional-client-side