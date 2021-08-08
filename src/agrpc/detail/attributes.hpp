// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_ATTRIBUTES_HPP
#define AGRPC_DETAIL_ATTRIBUTES_HPP

#include <version>

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(unlikely)
#if ((defined(_MSVC_LANG) && _MSVC_LANG > 201703L) || __cplusplus > 201703L)
#define AGRPC_UNLIKELY [[unlikely]]
#endif
#endif
#endif
#ifndef AGRPC_UNLIKELY
#define AGRPC_UNLIKELY
#endif

#endif  // AGRPC_DETAIL_ATTRIBUTES_HPP
