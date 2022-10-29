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

#ifndef AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP
#define AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/intrusive_list.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/health_check_service.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct IntrusiveListHook
{
    detail::IntrusiveListHook* next;
    detail::IntrusiveListHook* prev;
};

using HealthCheckWatcherList = detail::IntrusiveList<detail::IntrusiveListHook>;

struct HealthCheckServiceData
{
    detail::ServingStatus status{detail::ServingStatus::NOT_FOUND};
    detail::HealthCheckWatcherList watchers;
};

inline auto to_grpc_serving_status(detail::ServingStatus status)
{
    return status == detail::ServingStatus::NOT_FOUND
               ? grpc::health::v1::HealthCheckResponse_ServingStatus::HealthCheckResponse_ServingStatus_SERVICE_UNKNOWN
           : status == detail::ServingStatus::SERVING
               ? grpc::health::v1::HealthCheckResponse_ServingStatus::HealthCheckResponse_ServingStatus_SERVING
               : grpc::health::v1::HealthCheckResponse_ServingStatus::HealthCheckResponse_ServingStatus_NOT_SERVING;
}

class HealthCheckWatcher : public detail::TypeErasedGrpcTagOperation, public detail::IntrusiveListHook
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;

  public:
    explicit HealthCheckWatcher(agrpc::HealthCheckService& service)
        : GrpcBase(&HealthCheckWatcher::on_complete), service(service)
    {
    }

    void run()
    {
        auto& service_data = service.services_map_[request.service()];
        service_data.watchers.push_back(this);
        send_health(service_data.status);
    }

    void send_health(detail::ServingStatus status)
    {
        if (write_pending)
        {
            pending_status = status;
        }
        else if (!finish_called)
        {
            send_health_impl(status);
        }
    }

    static auto create_and_initiate(agrpc::HealthCheckService& service, void* tag)
    {
        auto& grpc_context = *service.grpc_context;
        auto watcher = detail::allocate<HealthCheckWatcher>(grpc_context.get_allocator(), service);
        grpc_context.work_started();
        watcher->initiate_request(tag);
        return watcher.release();
    }

    void deallocate() { detail::destroy_deallocate(this, get_allocator()); }

  private:
    agrpc::GrpcContext& grpc_context() const noexcept { return *service.grpc_context; }

    agrpc::GrpcContext::allocator_type get_allocator() const noexcept { return grpc_context().get_allocator(); }

    static void on_complete(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok, agrpc::GrpcContext&)
    {
        auto* self = static_cast<HealthCheckWatcher*>(op);
        detail::ScopeGuard guard{[&]
                                 {
                                     auto& service_data =
                                         self->service.services_map_.find(self->request.service())->second;
                                     service_data.watchers.remove(self);
                                     self->deallocate();
                                 }};
        if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
        {
            const auto write_pending = std::exchange(self->write_pending, false);
            if (write_pending)
            {
                guard.release();
            }
            if (ok)
            {
                if (!self->finish_called && self->pending_status != detail::ServingStatus::NOT_FOUND)
                {
                    const auto status = std::exchange(self->pending_status, detail::ServingStatus::NOT_FOUND);
                    self->send_health_impl(status);
                    guard.release();
                }
            }
            else if (write_pending && !self->finish_called)
            {
                self->finish(grpc::Status(grpc::StatusCode::CANCELLED, "OnWriteDone() ok=false"));
            }
        }
    }

    void initiate_request(void* tag)
    {
        auto* const cq = grpc_context().get_server_completion_queue();
        service.service.RequestWatch(&server_context, &request, &writer, cq, cq, tag);
    }

    void send_health_impl(detail::ServingStatus status)
    {
        if (service.is_shutdown)
        {
            finish(grpc::Status(grpc::StatusCode::CANCELLED, "not writing due to shutdown"));
            return;
        }
        write(status);
    }

    void write(detail::ServingStatus status)
    {
        write_pending = true;
        response.set_status(detail::to_grpc_serving_status(status));
        grpc_context().work_started();
        writer.Write(response, static_cast<GrpcBase*>(this));
    }

    void finish(const grpc::Status& status)
    {
        finish_called = true;
        grpc_context().work_started();
        writer.Finish(status, static_cast<GrpcBase*>(this));
    }

    agrpc::HealthCheckService& service;
    grpc::ServerContext server_context;
    grpc::health::v1::HealthCheckRequest request;
    grpc::health::v1::HealthCheckResponse response;
    grpc::ServerAsyncWriter<grpc::health::v1::HealthCheckResponse> writer{&server_context};
    detail::ServingStatus pending_status{detail::ServingStatus::NOT_FOUND};
    bool write_pending{};
    bool finish_called{};
};

class HealthCheckChecker : public detail::TypeErasedGrpcTagOperation
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;

  public:
    explicit HealthCheckChecker(agrpc::HealthCheckService& service)
        : GrpcBase(&HealthCheckChecker::on_complete), service(service)
    {
    }

    void run()
    {
        const auto status = service.get_serving_status(request.service());
        finish(status);
    }

    static auto create_and_initiate(agrpc::HealthCheckService& service, void* tag)
    {
        auto& grpc_context = *service.grpc_context;
        auto watcher = detail::allocate<HealthCheckChecker>(grpc_context.get_allocator(), service);
        grpc_context.work_started();
        watcher->initiate_request(tag);
        return watcher.release();
    }

    void deallocate() { detail::destroy_deallocate(this, get_allocator()); }

  private:
    agrpc::GrpcContext& grpc_context() const noexcept { return *service.grpc_context; }

    agrpc::GrpcContext::allocator_type get_allocator() const noexcept { return grpc_context().get_allocator(); }

    static void on_complete(GrpcBase* op, detail::InvokeHandler, bool, agrpc::GrpcContext&)
    {
        auto* self = static_cast<HealthCheckChecker*>(op);
        self->deallocate();
    }

    void initiate_request(void* tag)
    {
        auto* const cq = grpc_context().get_server_completion_queue();
        service.service.RequestCheck(&server_context, &request, &writer, cq, cq, tag);
    }

    void finish(detail::ServingStatus status)
    {
        grpc_context().work_started();
        if (detail::ServingStatus::NOT_FOUND == status)
        {
            writer.FinishWithError(grpc::Status(grpc::StatusCode::NOT_FOUND, "service name unknown"), this);
            return;
        }
        grpc::health::v1::HealthCheckResponse response;
        response.set_status(detail::to_grpc_serving_status(status));
        writer.Finish(response, grpc::Status::OK, this);
    }

    agrpc::HealthCheckService& service;
    grpc::ServerContext server_context;
    grpc::health::v1::HealthCheckRequest request;
    grpc::ServerAsyncResponseWriter<grpc::health::v1::HealthCheckResponse> writer{&server_context};
};

template <class Implementation>
inline HealthCheckRepeatedlyRequest<Implementation>::HealthCheckRepeatedlyRequest(agrpc::HealthCheckService& service)
    : GrpcBase(&HealthCheckRepeatedlyRequest::on_request_complete), service(service)
{
}

template <class Implementation>
inline void HealthCheckRepeatedlyRequest<Implementation>::start()
{
    this->impl = Implementation::create_and_initiate(service, this);
}

template <class Implementation>
inline void HealthCheckRepeatedlyRequest<Implementation>::on_request_complete(GrpcBase* op,
                                                                              detail::InvokeHandler invoke_handler,
                                                                              bool ok, agrpc::GrpcContext&)
{
    auto* self = static_cast<HealthCheckRepeatedlyRequest*>(op);
    auto* const impl = self->impl;
    if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler && ok)
    {
        self->start();
        impl->run();
    }
    else
    {
        impl->deallocate();
    }
}

inline void set_serving_status(detail::HealthCheckServiceData& service_data, detail::ServingStatus status)
{
    service_data.status = status;
    for (auto& p : service_data.watchers)
    {
        static_cast<detail::HealthCheckWatcher&>(p).send_health(status);
    }
}
}

inline HealthCheckService::HealthCheckService(grpc::ServerBuilder& builder)
    : repeatedly_request_watch(*this), repeatedly_request_check(*this)
{
    services_map_[""].status = detail::ServingStatus::SERVING;
    builder.RegisterService(&service);
}

inline void HealthCheckService::SetServingStatus(const std::string& service_name, bool serving)
{
    detail::create_and_submit_no_arg_operation<false>(
        *grpc_context,
        [&, service_name, serving]() mutable
        {
            if (is_shutdown)
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
        *grpc_context,
        [&, serving]()
        {
            if (is_shutdown)
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
    detail::create_and_submit_no_arg_operation<false>(*grpc_context,
                                                      [&]()
                                                      {
                                                          if (is_shutdown)
                                                          {
                                                              return;
                                                          }
                                                          is_shutdown = true;
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
    return it == services_map_.end() ? detail::ServingStatus::NOT_FOUND : it->second.status;
}

inline grpc::ServerBuilder& add_health_check_service(grpc::ServerBuilder& builder)
{
    return builder.SetOption(std::make_unique<grpc::HealthCheckServiceServerBuilderOption>(
        std::make_unique<agrpc::HealthCheckService>(builder)));
}

inline void start_health_check_service(grpc::HealthCheckServiceInterface* service, agrpc::GrpcContext& grpc_context)
{
    auto& impl = *static_cast<agrpc::HealthCheckService*>(service);
    impl.grpc_context = &grpc_context;
    impl.repeatedly_request_watch.start();
    impl.repeatedly_request_check.start();
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP
