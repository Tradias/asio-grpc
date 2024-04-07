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

// config.hpp
#undef AGRPC_UNLIKELY
#undef AGRPC_LIKELY

#undef AGRPC_TRY
#undef AGRPC_CATCH

#undef AGRPC_NAMESPACE_BEGIN

#undef AGRPC_NAMESPACE_END

// asio_forward.hpp
#undef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#undef AGRPC_ASIO_HAS_BIND_ALLOCATOR
#undef AGRPC_ASIO_HAS_NEW_SPAWN
#undef AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR

// awaitable.hpp
#undef AGRPC_ASIO_HAS_CO_AWAIT