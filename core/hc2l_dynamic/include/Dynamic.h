#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <functional>
#include <chrono>
#include "road_network.h"
#include "coordinate_mapper.h"

namespace hc2l_dynamic {

// Define EdgeID as a pair to represent edges consistently
using EdgeID = std::pair<road_network::NodeID, road_network::NodeID>;

// Custom hash function for EdgeID (std::pair)
struct EdgeIDHasher {
    std::size_t operator()(const EdgeID& eid) const {
        return std::hash<road_network::NodeID>()(eid.first) ^ 
               (std::hash<road_network::NodeID>()(eid.second) << 1);
    }
};

enum class Mode { BASE, DISRUPTED, IMMEDIATE_UPDATE, LAZY_UPDATE };

// Structure to hold detailed route information
struct RouteInfo {
    road_network::distance_t total_distance;
    std::vector<road_network::NodeID> path;
    std::vector<std::string> road_names;
    std::vector<std::pair<double, double>> coordinates; // (lat, lng) pairs
    bool uses_disruptions;
    double estimated_time_minutes;
};

class Dynamic {
public:
    explicit Dynamic(road_network::Graph &baseGraph);

    // Initialize coordinate mapping system
    bool initializeCoordinateMapping(const std::string& nodes_csv_file, const std::string& scenario_csv_file);

    void loadDisruptions(const std::string &filename);
    void setMode(Mode mode);
    Mode getMode() const;

    // Main query API
    road_network::distance_t get_distance(road_network::NodeID v, road_network::NodeID w, bool weighted);
    
    // NEW: Get path with traversed nodes
    std::pair<road_network::distance_t, std::vector<road_network::NodeID>> get_path(road_network::NodeID v, road_network::NodeID w, bool weighted);
    
    // NEW: GPS-based routing with detailed information
    RouteInfo findRouteByGPS(double start_lat, double start_lng, double end_lat, double end_lng, bool weighted = true);
    
    // NEW: Enhanced route display with road names and coordinates
    void displayDetailedRoute(const RouteInfo& route) const;
    
    // NEW: Check if route uses disrupted edges
    bool route_uses_disruptions(const std::vector<road_network::NodeID>& path) const;

    // NEW: Get actual number of nodes visited during distance calculation
    size_t get_visited_nodes_count(road_network::NodeID v, road_network::NodeID w, bool weighted);

    // NEW: user-reported disruption support
    void addUserDisruption(road_network::NodeID u, road_network::NodeID v,
                           const std::string& incidentType, const std::string& severity);

    // NEW: Impact Score Evaluation System
    struct ImpactScore {
        double score;
        double f_delta_w;  // Speed drop factor
        double f_jam;      // Jam factor component
        double f_closure;  // Closure factor
        double network_percentage_affected;
        bool exceeds_threshold;
    };
    
    ImpactScore calculateImpactScore(double slowdown_ratio, double jamFactor, bool isClosed, double segment_length = 100.0) const;
    Mode determineUpdateMode(const ImpactScore& impact) const;
    double calculateNetworkImpactPercentage() const;
    
    // NEW: Proper Lazy/Immediate Update System
    void markLabelsStale(const std::vector<road_network::NodeID>& affected_nodes);
    void repairStaleLabels(road_network::NodeID u, road_network::NodeID v);
    void triggerBackgroundLabelUpdate();
    bool areLabelsStale(road_network::NodeID u, road_network::NodeID v) const;
    void precomputeAffectedLabels();

private:
    road_network::Graph &graph;
    Mode currentMode;
    
    // Coordinate mapping system
    CoordinateMapper coordinate_mapper;
    bool coordinate_mapping_initialized;

    std::unordered_set<EdgeID, EdgeIDHasher> disruptedClosedEdges;
    std::unordered_map<EdgeID, double, EdgeIDHasher> disruptedSlowdownFactorByEdge;
    std::unordered_map<EdgeID, std::string, EdgeIDHasher> disruptionSeverityByEdge;
    std::unordered_map<EdgeID, std::string, EdgeIDHasher> disruptionTypeByEdge;
    
    // NEW: Label staleness tracking for Lazy/Immediate modes
    std::unordered_set<road_network::NodeID> stale_nodes;
    std::unordered_map<std::pair<road_network::NodeID, road_network::NodeID>, bool, EdgeIDHasher> precomputed_labels;
    bool background_update_active;
    std::chrono::steady_clock::time_point last_update_time;

    // Helper method to check if a node is accessible (not completely isolated by disruptions)
    bool isNodeAccessible(road_network::NodeID node) const;
    
    // Find nearest node that is not completely cut off by disruptions
    road_network::NodeID findNearestAvailableNode(double lat, double lng, double& distance);
    
    // Check if a specific edge is disrupted
    bool isEdgeDisrupted(road_network::NodeID u, road_network::NodeID v) const;
    
    // Check if the route between two nodes is heavily disrupted (closed or severely slowed)
    bool isRouteHeavilyDisrupted(road_network::NodeID start, road_network::NodeID end) const;

    static EdgeID makeEdgeId(road_network::NodeID a, road_network::NodeID b);
};

} // namespace hc2l_dynamic
