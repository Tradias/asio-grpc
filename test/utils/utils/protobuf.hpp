// Copyright 2022 Dennis Hezel
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
#include <grpcpp/grpcpp.h>

namespace test
{
template <class Message>
Message grpc_buffer_to_message(grpc::ByteBuffer& buffer)
{
    Message message;
    grpc::ProtoBufferReader reader{&buffer};
    CHECK(message.ParseFromZeroCopyStream(&reader));
    return message;
}

template <class Message>
grpc::ByteBuffer message_to_grpc_buffer(const Message& message)
{
    grpc::ByteBuffer buffer;
    const auto message_byte_size = static_cast<int>(message.ByteSizeLong());
    grpc::ProtoBufferWriter writer{&buffer, message_byte_size, message_byte_size};
    CHECK(message.SerializeToZeroCopyStream(&writer));
    return buffer;
}
}

#endif  // AGRPC_UTILS_PROTOBUF_HPP
