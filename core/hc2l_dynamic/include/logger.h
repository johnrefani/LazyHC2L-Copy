// ===== logger.h =====
#pragma once

#include <string>
#include <fstream>
#include <tuple>
#include <vector>
#include <unordered_map>
#include "road_network.h"

namespace logger {

// Call this once to create the CSV with headers
void init_logger(const std::string &filename);

// Call this per experiment/query to log metrics
void log_experiment(
    const std::string &mode,              // "BASE" or "DISRUPTED"
    road_network::NodeID source,
    road_network::NodeID target,
    double distance,
    double runtime_ms,
    const std::string &severity,         // "Light", "Medium", etc.
    const std::string &incident_type,    // "Accident", etc.
    double slowdown,
    double tau_threshold,
    bool used_lazy_fallback              // true if used lazy repair
);

} // namespace logger
