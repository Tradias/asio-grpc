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

#ifndef AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP
#define AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/create_and_submit_no_arg_operation.hpp>
#include <agrpc/detail/intrusive_list.hpp>
#include <agrpc/detail/intrusive_list_hook.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/server_write_reactor.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/health_check_service.hpp>
#include <grpc/support/log.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/async_unary_call.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using HealthCheckWatcherList = detail::IntrusiveList<detail::HealthCheckWatcher>;

struct HealthCheckServiceData
{
    detail::ServingStatus status_{detail::ServingStatus::NOT_FOUND};
    detail::HealthCheckWatcherList watchers_;
};

inline auto to_grpc_serving_status(detail::ServingStatus status)
{
    return status == detail::ServingStatus::NOT_FOUND
               ? grpc::health::v1::HealthCheckResponse_ServingStatus::HealthCheckResponse_ServingStatus_SERVICE_UNKNOWN
           : status == detail::ServingStatus::SERVING
               ? grpc::health::v1::HealthCheckResponse_ServingStatus::HealthCheckResponse_ServingStatus_SERVING
               : grpc::health::v1::HealthCheckResponse_ServingStatus::HealthCheckResponse_ServingStatus_NOT_SERVING;
}

class HealthCheckWatcher : public detail::IntrusiveListHook<detail::HealthCheckWatcher>,
                           public detail::ServerWriteReactor<HealthCheckWatcher, grpc::health::v1::HealthCheckResponse>
{
  private:
    using Base = detail::ServerWriteReactor<HealthCheckWatcher, grpc::health::v1::HealthCheckResponse>;

  public:
    explicit HealthCheckWatcher(agrpc::HealthCheckService& service, void* tag)
        : Base(*service.grpc_context_, &grpc::health::v1::Health::AsyncService::RequestWatch, service.service_,
               request_, tag),
          service_(service)
    {
    }

    void run()
    {
        auto& service_data = service_.services_map_[request_.service()];
        service_data.watchers_.push_back(this);
        send_health(service_data.status_);
    }

    void send_health(detail::ServingStatus status)
    {
        if (this->is_writing())
        {
            pending_status_ = status;
        }
        else if (!this->is_finishing())
        {
            send_health_impl(status);
        }
    }

    static auto create_and_initiate(agrpc::HealthCheckService& service, void* tag)
    {
        auto& grpc_context = *service.grpc_context_;
        return Base::create(grpc_context, service, tag);
    }

  private:
    friend Base;

    void on_write_done(bool ok)
    {
        if (ok)
        {
            if (pending_status_ != detail::ServingStatus::NOT_FOUND)
            {
                const auto status = std::exchange(pending_status_, detail::ServingStatus::NOT_FOUND);
                send_health_impl(status);
            }
        }
        else
        {
            this->finish(grpc::Status(grpc::StatusCode::CANCELLED, "OnWriteDone() ok=false"));
        }
    }

    void on_done()
    {
        const auto it = service_.services_map_.find(request_.service());
        auto& [status, watchers] = it->second;
        watchers.remove(this);
        if (status == detail::ServingStatus::NOT_FOUND && watchers.empty())
        {
            service_.services_map_.erase(it);
        }
    }

    void send_health_impl(detail::ServingStatus status)
    {
        response_.set_status(detail::to_grpc_serving_status(status));
        this->write(response_);
    }

    agrpc::HealthCheckService& service_;
    grpc::health::v1::HealthCheckRequest request_;
    grpc::health::v1::HealthCheckResponse response_;
    detail::ServingStatus pending_status_{detail::ServingStatus::NOT_FOUND};
};

class HealthCheckChecker : public detail::OperationBase
{
  private:
    using Base = detail::OperationBase;

  public:
    explicit HealthCheckChecker(agrpc::HealthCheckService& service, void* tag)
        : Base(&HealthCheckChecker::do_complete), service_(service)
    {
        auto* const cq = grpc_context().get_server_completion_queue();
        service_.service_.RequestCheck(&server_context_, &request_, &writer_, cq, cq, tag);
    }

    void run() { finish(service_.get_serving_status(request_.service())); }

    static auto create_and_initiate(agrpc::HealthCheckService& service, void* tag)
    {
        auto& grpc_context = *service.grpc_context_;
        return detail::allocate<HealthCheckChecker>(grpc_context.get_allocator(), service, tag).release();
    }

    void deallocate() { detail::destroy_deallocate(this, get_allocator()); }

  private:
    agrpc::GrpcContext& grpc_context() const noexcept { return *service_.grpc_context_; }

    agrpc::GrpcContext::allocator_type get_allocator() const noexcept { return grpc_context().get_allocator(); }

    static void do_complete(Base* op, detail::OperationResult, agrpc::GrpcContext& grpc_context)
    {
        auto* self = static_cast<HealthCheckChecker*>(op);
        grpc_context.work_started();
        self->deallocate();
    }

    void finish(detail::ServingStatus status)
    {
        if (detail::ServingStatus::NOT_FOUND == status)
        {
            writer_.FinishWithError(grpc::Status(grpc::StatusCode::NOT_FOUND, "service name unknown"), this);
            return;
        }
        grpc::health::v1::HealthCheckResponse response;
        response.set_status(detail::to_grpc_serving_status(status));
        writer_.Finish(response, grpc::Status::OK, this);
    }

    agrpc::HealthCheckService& service_;
    grpc::ServerContext server_context_;
    grpc::health::v1::HealthCheckRequest request_;
    grpc::ServerAsyncResponseWriter<grpc::health::v1::HealthCheckResponse> writer_{&server_context_};
};

template <class Implementation>
inline HealthCheckRepeatedlyRequest<Implementation>::HealthCheckRepeatedlyRequest(agrpc::HealthCheckService& service)
    : Base(&HealthCheckRepeatedlyRequest::do_request_complete), service_(service)
{
}

template <class Implementation>
inline void HealthCheckRepeatedlyRequest<Implementation>::start()
{
    impl_ = Implementation::create_and_initiate(service_, this);
}

template <class Implementation>
inline void HealthCheckRepeatedlyRequest<Implementation>::do_request_complete(Base* op, detail::OperationResult result,
                                                                              agrpc::GrpcContext&)
{
    auto* self = static_cast<HealthCheckRepeatedlyRequest*>(op);
    self->service_.grpc_context_->work_started();
    auto* const impl = self->impl_;
    if AGRPC_LIKELY (detail::OperationResult::OK_ == result || detail::OperationResult::SHUTDOWN_OK == result)
    {
        if AGRPC_LIKELY (detail::OperationResult::OK_ == result)
        {
            self->start();
            impl->run();
        }
    }
    else
    {
        impl->deallocate();
    }
}

inline void set_serving_status(detail::HealthCheckServiceData& service_data, detail::ServingStatus status)
{
    service_data.status_ = status;
    for (auto& watcher : service_data.watchers_)
    {
        watcher.send_health(status);
    }
}
}

inline HealthCheckService::HealthCheckService(grpc::ServerBuilder& builder)
    : repeatedly_request_watch_(*this), repeatedly_request_check_(*this)
{
    services_map_[""].status_ = detail::ServingStatus::SERVING;
    builder.RegisterService(&service_);
}

inline void HealthCheckService::SetServingStatus(const std::string& service_name, bool serving)
{
    detail::create_and_submit_no_arg_operation<false>(
        *grpc_context_,
        [&, service_name, serving]() mutable
        {
            if (is_shutdown_)
            {
                // Set to NOT_SERVING in case service_name is not in the map.
                serving = false;
            }
            detail::set_serving_status(services_map_[service_name],
                                       serving ? detail::ServingStatus::SERVING : detail::ServingStatus::NOT_SERVING);
        });
}

inline void HealthCheckService::SetServingStatus(bool serving)
{
    detail::create_and_submit_no_arg_operation<false>(
        *grpc_context_,
        [&, serving]()
        {
            if (is_shutdown_)
            {
                return;
            }
            const auto status = serving ? detail::ServingStatus::SERVING : detail::ServingStatus::NOT_SERVING;
            for (auto& p : services_map_)
            {
                detail::set_serving_status(p.second, status);
            }
        });
}

inline void HealthCheckService::Shutdown()
{
    detail::create_and_submit_no_arg_operation<false>(*grpc_context_,
                                                      [&]()
                                                      {
                                                          if (is_shutdown_)
                                                          {
                                                              return;
                                                          }
                                                          is_shutdown_ = true;
                                                          for (auto& p : services_map_)
                                                          {
                                                              detail::set_serving_status(
                                                                  p.second, detail::ServingStatus::NOT_SERVING);
                                                          }
                                                      });
}

detail::ServingStatus HealthCheckService::get_serving_status(const std::string& service_name) const
{
    const auto it = services_map_.find(service_name);
    return it == services_map_.end() ? detail::ServingStatus::NOT_FOUND : it->second.status_;
}

inline grpc::ServerBuilder& add_health_check_service(grpc::ServerBuilder& builder)
{
    return builder.SetOption(std::make_unique<grpc::HealthCheckServiceServerBuilderOption>(
        std::make_unique<agrpc::HealthCheckService>(builder)));
}

inline void start_health_check_service(agrpc::HealthCheckService& service, agrpc::GrpcContext& grpc_context)
{
    service.grpc_context_ = &grpc_context;
    service.repeatedly_request_watch_.start();
    service.repeatedly_request_check_.start();
}

inline void start_health_check_service(grpc::Server& server, agrpc::GrpcContext& grpc_context)
{
    auto* const service = server.GetHealthCheckService();
    // clang-format off
    GPR_ASSERT(service && "Use `agrpc::add_health_check_service` to add the HealthCheckService to a ServerBuilder before calling this function");
    // clang-format on
    agrpc::start_health_check_service(*static_cast<agrpc::HealthCheckService*>(service), grpc_context);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP
