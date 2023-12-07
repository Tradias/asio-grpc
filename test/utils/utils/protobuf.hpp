// Copyright 2023 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AGRPC_UTILS_PROTOBUF_HPP
#define AGRPC_UTILS_PROTOBUF_HPP

#include <doctest/doctest.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/support/proto_buffer_reader.h>
#include <grpcpp/support/proto_buffer_writer.h>

namespace test
{
template <class Message>
Message grpc_buffer_to_message(grpc::ByteBuffer& buffer)
{
    Message message;
    const auto status = grpc::GenericDeserialize<grpc::ProtoBufferReader, Message>(&buffer, &message);
    CHECK_MESSAGE(status.ok(), status.error_message());
    return message;
}

template <class Message>
grpc::ByteBuffer message_to_grpc_buffer(const Message& message)
{
    grpc::ByteBuffer buffer;
    bool own_buffer;
    const auto status = grpc::GenericSerialize<grpc::ProtoBufferWriter, Message>(message, &buffer, &own_buffer);
    CHECK_MESSAGE(status.ok(), status.error_message());
    return buffer;
}
}

#endif  // AGRPC_UTILS_PROTOBUF_HPP
