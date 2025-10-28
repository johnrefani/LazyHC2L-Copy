#include "logger.h"
#include "road_network.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace logger {

namespace fs = std::filesystem;

std::string log_file_path = "../logs/experiment_results.csv";

void init_logger(const std::string& path) {
    log_file_path = path;

    // Ensure directory exists
    fs::path p(log_file_path);
    fs::create_directories(p.parent_path());

    std::ofstream file(log_file_path, std::ios::out);
    if (!file.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << log_file_path << std::endl;
        return;
    }

    // Write header
    file << "mode,source,target,distance_ms,response_time_ms,severity,incident_type,slowdown,tau,fallback_triggered\n";
    file.close();
}

void log_experiment(
    const std::string &mode,
    road_network::NodeID source,
    road_network::NodeID target,
    double distance,
    double runtime_ms,
    const std::string &severity,
    const std::string &incident_type,
    double slowdown,
    double tau_threshold,
    bool used_lazy_fallback
) {
    std::ofstream file(log_file_path, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "[Logger] Failed to write to log file: " << log_file_path << std::endl;
        return;
    }

    file << mode << ","
         << source << ","
         << target << ","
         << distance << ","
         << runtime_ms << ","
         << severity << ","
         << incident_type << ","
         << slowdown << ","
         << tau_threshold << ","
         << (used_lazy_fallback ? "1" : "0") << "\n";

    file.close();
}

} // logger
