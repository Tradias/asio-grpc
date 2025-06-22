// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_REACTOR_PTR_TYPE_HPP
#define AGRPC_DETAIL_REACTOR_PTR_TYPE_HPP

#include <agrpc/detail/forward.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Reactor>
struct RefCountedReactorType
{
    using Type = Reactor;
};

template <class Executor>
struct RefCountedReactorType<agrpc::BasicServerUnaryReactor<Executor>>
{
    using Type = RefCountedServerReactor<agrpc::BasicServerUnaryReactor<Executor>>;
};

template <class Request, class Executor>
struct RefCountedReactorType<agrpc::BasicServerReadReactor<Request, Executor>>
{
    using Type = RefCountedServerReactor<agrpc::BasicServerReadReactor<Request, Executor>>;
};

template <class Response, class Executor>
struct RefCountedReactorType<agrpc::BasicServerWriteReactor<Response, Executor>>
{
    using Type = RefCountedServerReactor<agrpc::BasicServerWriteReactor<Response, Executor>>;
};

template <class Request, class Response, class Executor>
struct RefCountedReactorType<agrpc::BasicServerBidiReactor<Request, Response, Executor>>
{
    using Type = RefCountedServerReactor<agrpc::BasicServerBidiReactor<Request, Response, Executor>>;
};

template <class Executor>
struct RefCountedReactorType<agrpc::BasicClientUnaryReactor<Executor>>
{
    using Type = RefCountedClientReactor<agrpc::BasicClientUnaryReactor<Executor>>;
};

template <class Request, class Executor>
struct RefCountedReactorType<agrpc::BasicClientWriteReactor<Request, Executor>>
{
    using Type = RefCountedClientReactor<agrpc::BasicClientWriteReactor<Request, Executor>>;
};

template <class Response, class Executor>
struct RefCountedReactorType<agrpc::BasicClientReadReactor<Response, Executor>>
{
    using Type = RefCountedClientReactor<agrpc::BasicClientReadReactor<Response, Executor>>;
};

template <class Request, class Response, class Executor>
struct RefCountedReactorType<agrpc::BasicClientBidiReactor<Request, Response, Executor>>
{
    using Type = RefCountedClientReactor<agrpc::BasicClientBidiReactor<Request, Response, Executor>>;
};

template <class Reactor>
using RefCountedReactorTypeT = typename RefCountedReactorType<Reactor>::Type;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REACTOR_PTR_TYPE_HPP
