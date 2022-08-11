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

#ifndef AGRPC_DETAIL_MEMORY_RESOURCE_HPP
#define AGRPC_DETAIL_MEMORY_RESOURCE_HPP

#include <agrpc/detail/config.hpp>

#ifdef AGRPC_USE_BOOST_CONTAINER
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <boost/container/uses_allocator.hpp>
#else
#include <memory_resource>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#ifdef AGRPC_USE_BOOST_CONTAINER
namespace pmr = boost::container::pmr;
namespace container = boost::container;
#else
namespace pmr = std::pmr;
namespace container = std;
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_RESOURCE_HPP
