#pragma once

#include <string>

#include "envoy/common/pure.h"
#include "envoy/event/dispatcher.h"
#include "envoy/event/scaled_range_timer_manager.h"
#include "envoy/server/overload/thread_local_overload_state.h"

#include "source/common/singleton/const_singleton.h"

namespace Envoy {
namespace Server {
/**
 * Well-known overload action names.
 */
class OverloadActionNameValues {
public:
  // Overload action to stop accepting new HTTP requests.
  const std::string StopAcceptingRequests = "envoy.overload_actions.stop_accepting_requests";

  // Overload action to disable http keepalive (for HTTP1.x).
  const std::string DisableHttpKeepAlive = "envoy.overload_actions.disable_http_keepalive";

  // Overload action to stop accepting new connections.
  const std::string StopAcceptingConnections = "envoy.overload_actions.stop_accepting_connections";

  // Overload action to reject (accept and then close) new connections.
  const std::string RejectIncomingConnections =
      "envoy.overload_actions.reject_incoming_connections";

  // Overload action to try to shrink the heap by releasing free memory.
  const std::string ShrinkHeap = "envoy.overload_actions.shrink_heap";

  // Overload action to reduce some subset of configured timeouts.
  const std::string ReduceTimeouts = "envoy.overload_actions.reduce_timeouts";

  // Overload action to reset streams using excessive memory.
  const std::string ResetStreams = "envoy.overload_actions.reset_high_memory_stream";
};

using OverloadActionNames = ConstSingleton<OverloadActionNameValues>;

/**
 * Well-known overload action stats.
 */
class OverloadActionStatsNameValues {
public:
  // Count of the number of streams the reset streams action has reset
  const std::string ResetStreamsCount = "envoy.overload_actions.reset_high_memory_stream.count";
};

using OverloadActionStatsNames = ConstSingleton<OverloadActionStatsNameValues>;

/**
 * A point within the connection or request lifecycle that provides context on
 * whether to shed load at that given stage for the current entity at the point.
 */
class LoadShedPoint {
public:
  virtual ~LoadShedPoint() = default;

  // Whether to shed the load.
  virtual bool shouldShedLoad() PURE;
};

using LoadShedPointPtr = std::unique_ptr<LoadShedPoint>;

/**
 * The OverloadManager protects the Envoy instance from being overwhelmed by client
 * requests. It monitors a set of resources and notifies registered listeners if
 * configured thresholds for those resources have been exceeded.
 */
class OverloadManager {
public:
  virtual ~OverloadManager() = default;

  /**
   * Start a recurring timer to monitor resources and notify listeners when overload actions
   * change state.
   */
  virtual void start() PURE;

  /**
   * Register a callback to be invoked when the specified overload action changes state
   * (i.e., becomes activated or inactivated). Must be called before the start method is called.
   * @param action const std::string& the name of the overload action to register for
   * @param dispatcher Event::Dispatcher& the dispatcher on which callbacks will be posted
   * @param callback OverloadActionCb the callback to post when the overload action
   *        changes state
   * @returns true if action was registered and false if no such action has been configured
   */
  virtual bool registerForAction(const std::string& action, Event::Dispatcher& dispatcher,
                                 OverloadActionCb callback) PURE;

  /**
   * Get the thread-local overload action states. Lookups in this object can be used as
   * an alternative to registering a callback for overload action state changes.
   */
  virtual ThreadLocalOverloadState& getThreadLocalOverloadState() PURE;

  /**
   * Get the load shed point identified by the following string. Returns nullptr
   * on for non-configured points.
   */
  virtual LoadShedPoint* getLoadShedPoint(absl::string_view point_name) PURE;

  /**
   * Get a factory for constructing scaled timer managers that respond to overload state.
   */
  virtual Event::ScaledRangeTimerManagerFactory scaledTimerFactory() PURE;
};

} // namespace Server
} // namespace Envoy
