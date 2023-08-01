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

#ifndef AGRPC_DETAIL_NAMESPACE_CPP20_HPP
#define AGRPC_DETAIL_NAMESPACE_CPP20_HPP

#include <agrpc/detail/config.hpp>

#if defined(ASIO_HAS_CO_AWAIT) || defined(BOOST_ASIO_HAS_CO_AWAIT)
#define AGRPC_NAMESPACE_CPP20_BEGIN() \
    inline namespace cpp20            \
    {

#define AGRPC_NAMESPACE_CPP20_END }
#else
#define AGRPC_NAMESPACE_CPP20_BEGIN()

#define AGRPC_NAMESPACE_CPP20_END
#endif

#endif  // AGRPC_DETAIL_NAMESPACE_CPP20_HPP
