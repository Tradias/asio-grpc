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

#ifndef AGRPC_DETAIL_SERVER_CALLBACK_PTR_HPP
#define AGRPC_DETAIL_SERVER_CALLBACK_PTR_HPP

#include <agrpc/detail/reactor_ptr.hpp>
#include <agrpc/server_callback.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Executor>
class BasicRefCountedServerUnaryReactor final : public BasicServerUnaryReactor<Executor>
{
  private:
    using Base = BasicServerUnaryReactor<Executor>;
    using RefCountGuard = detail::RefCountGuard<BasicRefCountedServerUnaryReactor>;

  public:
    BasicRefCountedServerUnaryReactor(ReactorPtrDeallocateFn deallocate, Executor executor)
        : Base(static_cast<Executor&&>(executor)), deallocate_(deallocate)
    {
    }

    using Base::increment_ref_count;

    void decrement_ref_count() noexcept
    {
        if (Base::decrement_ref_count())
        {
            if (this->is_finished())
            {
                deallocate_(this);
            }
            else
            {
                this->initiate_finish({grpc::StatusCode::CANCELLED, {}});
            }
        }
    }

  private:
    void OnSendInitialMetadataDone(bool ok) override { this->on_send_initial_metadata_done(ok); }

    void OnDone() override
    {
        RefCountGuard g{*this};
        this->on_done();
    }

    void OnCancel() override { this->on_cancel(); }

    ReactorPtrDeallocateFn deallocate_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_CALLBACK_PTR_HPP
