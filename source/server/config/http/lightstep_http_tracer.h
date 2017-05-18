#pragma once

#include <string>

#include "envoy/server/instance.h"
#include "server/configuration_impl.h"

namespace Envoy {
namespace Server {
namespace Configuration {

/**
 * Config registration for the lightstep tracer. @see HttpTracerFactory.
 */
class LightstepTracerFactory : public HttpTracerFactory {
public:
  // HttpTracerFactory
  Tracing::HttpTracerPtr tryCreateHttpTracer(const std::string &type,
                                    const Json::Object& json_config,
                                    Server::Instance& server,
                                    Upstream::ClusterManager &cluster_manager);
};

} // Configuration
} // Server
} // Envoy