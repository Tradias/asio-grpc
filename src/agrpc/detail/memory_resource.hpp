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

#ifdef AGRPC_USE_RECYCLING_ALLOCATOR
#include <agrpc/detail/memory_resource_recycling_allocator.hpp>
#elif defined(AGRPC_USE_BOOST_CONTAINER)
#include <agrpc/detail/memory_resource_boost_pmr.hpp>
#else
#include <agrpc/detail/memory_resource_std_pmr.hpp>
#endif
