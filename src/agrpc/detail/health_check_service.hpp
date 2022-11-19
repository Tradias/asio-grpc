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
#include <agrpc/detail/intrusive_list_hook.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/server_write_reactor.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/health_check_service.hpp>
#include <grpcpp/server_context.h>

#include <cassert>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using HealthCheckWatcherList = detail::IntrusiveList<detail::HealthCheckWatcher>;

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

class HealthCheckWatcher : public detail::IntrusiveListHook<detail::HealthCheckWatcher>,
                           public detail::ServerWriteReactor<HealthCheckWatcher, grpc::health::v1::HealthCheckResponse>
{
  private:
    using Base = detail::ServerWriteReactor<HealthCheckWatcher, grpc::health::v1::HealthCheckResponse>;

  public:
    explicit HealthCheckWatcher(agrpc::HealthCheckService& service, void* tag)
        : Base(*service.grpc_context, &grpc::health::v1::Health::AsyncService::RequestWatch, service.service, request,
               tag),
          service(service)
    {
    }

    void run()
    {
        auto& service_data = service.services_map[request.service()];
        service_data.watchers.push_back(this);
        send_health(service_data.status);
    }

    void send_health(detail::ServingStatus status)
    {
        if (this->is_writing())
        {
            pending_status = status;
        }
        else if (!this->is_finished())
        {
            send_health_impl(status);
        }
    }

    static auto create_and_initiate(agrpc::HealthCheckService& service, void* tag)
    {
        auto& grpc_context = *service.grpc_context;
        return Base::create(grpc_context, service, tag);
    }

  private:
    friend Base;

    void on_write_done(bool ok)
    {
        if (ok)
        {
            if (pending_status != detail::ServingStatus::NOT_FOUND)
            {
                const auto status = std::exchange(pending_status, detail::ServingStatus::NOT_FOUND);
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
        const auto it = service.services_map.find(request.service());
        auto& [status, watchers] = it->second;
        watchers.remove(this);
        if (status == detail::ServingStatus::NOT_FOUND && watchers.empty())
        {
            service.services_map.erase(it);
        }
    }

    void send_health_impl(detail::ServingStatus status)
    {
        response.set_status(detail::to_grpc_serving_status(status));
        this->write(response);
    }

    agrpc::HealthCheckService& service;
    grpc::health::v1::HealthCheckRequest request;
    grpc::health::v1::HealthCheckResponse response;
    detail::ServingStatus pending_status{detail::ServingStatus::NOT_FOUND};
};

class HealthCheckChecker : public detail::OperationBase
{
  private:
    using Base = detail::OperationBase;

  public:
    explicit HealthCheckChecker(agrpc::HealthCheckService& service, void* tag)
        : Base(&HealthCheckChecker::do_complete), service(service)
    {
        auto* const cq = grpc_context().get_server_completion_queue();
        service.service.RequestCheck(&server_context, &request, &writer, cq, cq, tag);
    }

    void run() { finish(service.get_serving_status(request.service())); }

    static auto create_and_initiate(agrpc::HealthCheckService& service, void* tag)
    {
        auto& grpc_context = *service.grpc_context;
        return detail::allocate<HealthCheckChecker>(grpc_context.get_allocator(), service, tag).release();
    }

    void deallocate() { detail::destroy_deallocate(this, get_allocator()); }

  private:
    agrpc::GrpcContext& grpc_context() const noexcept { return *service.grpc_context; }

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
    : Base(&HealthCheckRepeatedlyRequest::do_request_complete), service(service)
{
}

template <class Implementation>
inline void HealthCheckRepeatedlyRequest<Implementation>::start()
{
    this->impl = Implementation::create_and_initiate(service, this);
}

template <class Implementation>
inline void HealthCheckRepeatedlyRequest<Implementation>::do_request_complete(Base* op, detail::OperationResult result,
                                                                              agrpc::GrpcContext&)
{
    auto* self = static_cast<HealthCheckRepeatedlyRequest*>(op);
    self->service.grpc_context->work_started();
    auto* const impl = self->impl;
    if AGRPC_LIKELY (detail::OperationResult::OK == result || detail::OperationResult::SHUTDOWN_OK == result)
    {
        if AGRPC_LIKELY (detail::OperationResult::OK == result)
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
    service_data.status = status;
    for (auto& watcher : service_data.watchers)
    {
        watcher.send_health(status);
    }
}
}

inline HealthCheckService::HealthCheckService(grpc::ServerBuilder& builder)
    : repeatedly_request_watch(*this), repeatedly_request_check(*this)
{
    services_map[""].status = detail::ServingStatus::SERVING;
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
            detail::set_serving_status(services_map[service_name],
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
            for (auto& p : services_map)
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
                                                          for (auto& p : services_map)
                                                          {
                                                              detail::set_serving_status(
                                                                  p.second, detail::ServingStatus::NOT_SERVING);
                                                          }
                                                      });
}

detail::ServingStatus HealthCheckService::get_serving_status(const std::string& service_name) const
{
    const auto it = services_map.find(service_name);
    return it == services_map.end() ? detail::ServingStatus::NOT_FOUND : it->second.status;
}

inline grpc::ServerBuilder& add_health_check_service(grpc::ServerBuilder& builder)
{
    return builder.SetOption(std::make_unique<grpc::HealthCheckServiceServerBuilderOption>(
        std::make_unique<agrpc::HealthCheckService>(builder)));
}

inline void start_health_check_service(agrpc::HealthCheckService& service, agrpc::GrpcContext& grpc_context)
{
    service.grpc_context = &grpc_context;
    service.repeatedly_request_watch.start();
    service.repeatedly_request_check.start();
}

inline void start_health_check_service(grpc::Server& server, agrpc::GrpcContext& grpc_context)
{
    auto* const service = server.GetHealthCheckService();
    assert(service &&
           "Use `agrpc::add_health_check_service` to add the HealthCheckService to a ServerBuilder before calling this "
           "function");
    if (service == nullptr)
    {
        return;
    }
    agrpc::start_health_check_service(*static_cast<agrpc::HealthCheckService*>(service), grpc_context);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HEALTH_CHECK_SERVICE_IPP
