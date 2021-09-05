/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef TEST_QPS_SERVER_H
#define TEST_QPS_SERVER_H

#include "protos/messages.pb.h"

#include <memory>

namespace grpc
{
namespace testing
{
class Server
{
  public:
    static bool SetPayload(PayloadType type, int size, Payload* payload)
    {
        if (type != PayloadType::COMPRESSABLE)
        {
            return false;
        }
        payload->set_type(type);
        // Don't waste time creating a new payload of identical size.
        if (payload->body().length() != static_cast<size_t>(size))
        {
            std::unique_ptr<char[]> body(new char[size]());
            payload->set_body(body.get(), size);
        }
        return true;
    }

    static Status SetResponse(const SimpleRequest* request, SimpleResponse* response)
    {
        if (request->response_size() > 0)
        {
            if (!Server::SetPayload(request->response_type(), request->response_size(), response->mutable_payload()))
            {
                return Status(grpc::StatusCode::INTERNAL, "Error creating payload.");
            }
        }
        return Status::OK;
    }
};
}  // namespace testing
}  // namespace grpc

#endif
