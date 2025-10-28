#ifndef GPS_ROUTING_SERVICE_H
#define GPS_ROUTING_SERVICE_H

#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include "include/Dynamic.h"
#include "include/road_network.h"

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;

// Structure to hold routing results
struct RoutingResult {
    bool success;
    string error_message;
    
    // Timing information
    long query_time_microseconds;
    
    // Distance information
    double total_distance_meters;
    
    // Path information
    vector<unsigned int> path;
    size_t path_length;
    
    // GPS to Node conversion
    int start_node;
    int dest_node;
    string gps_to_node_info;
    
    // Complete route trace
    string complete_route_trace;
    
    // Mode information
    string routing_mode;
    
    // Additional info for disrupted mode
    bool had_disruptions;
    double base_distance_meters;
    double distance_difference_meters;
    double distance_change_percentage;
    string route_comparison;
    
    // NEW: Labeling metrics
    double labeling_size_mb;
    double labeling_time_seconds;
};

class GPSRoutingService {
private:
    Dynamic* qc_router;
    Graph graph;
    bool initialized;
    
    // NEW: Store labeling metrics
    mutable double labeling_size_mb;
    mutable double labeling_time_seconds;
    mutable bool labeling_metrics_computed;
    
    string formatRouteTrace(const vector<unsigned int>& path);
    
public:
    GPSRoutingService();
    ~GPSRoutingService();
    
    // Initialize the routing service
    bool initialize(const string& graph_file = "../../data/processed/qc_from_csv.gr",
                   const string& nodes_file = "../../data/raw/quezon_city_nodes.csv",
                   const string& disruptions_file = "../../data/disruptions/qc_scenario_for_cpp_1.csv");
    
    // Main routing function
    RoutingResult findRoute(double start_latitude, double start_longitude,
                           double dest_latitude, double dest_longitude,
                           bool use_disrupted_mode = false);
    
    // Get labeling metrics
    double getLabelingSizeMB() const { return labeling_size_mb; }
    double getLabelingTimeSeconds() const { return labeling_time_seconds; }
    
    // Compute labeling metrics on demand (if not already computed)
    void computeLabelingMetrics();
    
    // Convenience method to get network statistics
    struct NetworkStats {
        string dataset_name = "Quezon City, Philippines";
        int total_intersections = 13649;
        int total_road_segments = 18768;
        string coverage = "Real GPS coordinates";
    };
    
    NetworkStats getNetworkStats();
    bool isInitialized() const;
};

// Utility function for demonstration
void demonstrateUsage();

#endif // GPS_ROUTING_SERVICE_H