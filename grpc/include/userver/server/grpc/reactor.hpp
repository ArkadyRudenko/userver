#pragma once

#include <memory>

#include <grpcpp/completion_queue.h>
#include <grpcpp/impl/codegen/service_type.h>

/// @file userver/server/grpc/reactor.hpp
/// @copybrief server::grpc::Reactor

USERVER_NAMESPACE_BEGIN

namespace server::grpc {

/// @brief Processes requests on a `CompletionQueue` using a handler
/// @note Must be destroyed after the corresponding `CompletionQueue`
class Reactor {
 public:
  Reactor& operator=(Reactor&&) = delete;
  virtual ~Reactor();

  /// Registers itself in the provided `ServerBuilder`
  virtual ::grpc::Service& GetService() = 0;

  /// Start serving requests
  virtual void Start() = 0;
};

}  // namespace server::grpc

USERVER_NAMESPACE_END
