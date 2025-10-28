#pragma once

#include <string>
#include <vector>
#include <memory>
#include "road_network.h"
#include "dhl_coordinate_mapper.h"

using namespace std;
using namespace road_network;

struct DHLRoutingResult {
    bool success = false;
    string error_message = "";
    
    // Performance metrics
    double query_time_microseconds = 0.0;
    double labeling_time_ms = 0.0;
    size_t labeling_size_bytes = 0;
    
    // Route information
    vector<NodeID> path;
    distance_t total_distance = 0;
    size_t path_length = 0;
    size_t hoplinks_examined = 0;
    
    // GPS mapping
    NodeID start_node = 0;
    NodeID dest_node = 0;
    string gps_to_node_info = "";
    
    // Route details
    string complete_route_trace = "";
    string routing_mode = "DHL";
    
    // Disruption handling
    bool uses_disruptions = false;
    vector<string> blocked_edges;
    vector<NodeID> blocked_nodes;
    
    // Index statistics
    size_t index_height = 0;
    double avg_cut_size = 0.0;
    size_t total_labels = 0;
    
    // Coordinate mapping
    double start_lat = 0.0, start_lng = 0.0;
    double dest_lat = 0.0, dest_lng = 0.0;
    double coordinate_threshold_meters = 1000.0;
    
    // Data sources information
    struct DataSources {
        string graph_file = "";
        string coordinates_file = "";
        string disruptions_file = "";
    } data_sources;
};

class DHLRoutingService {
private:
    unique_ptr<Graph> graph;
    unique_ptr<ContractionIndex> con_index;
    unique_ptr<ContractionHierarchy> ch;
    
    // Coordinate mapping system
    dhl::DHLCoordinateMapper coordinate_mapper;
    bool coordinate_mapping_initialized;
    
    // Disruption handling
    map<string, bool> disrupted_edges;
    set<NodeID> blocked_nodes;
    
    // Performance tracking
    double last_labeling_time_ms = 0.0;
    size_t last_labeling_size_bytes = 0;
    
    // Helper methods
    bool load_graph(const string& graph_file);
    bool build_index();
    
    // File path detection
    bool find_data_files(string& graph_file, string& coord_file, string& disruption_file) const;
    bool file_exists(const string& filepath) const;
    
    NodeID find_nearest_node(double lat, double lng, double threshold_meters = 1000.0) const;
    vector<NodeID> reconstruct_path(NodeID start, NodeID dest);
    vector<NodeID> dijkstra_with_path_reconstruction(NodeID start, NodeID dest);
    string create_route_trace(const vector<NodeID>& path) const;
    
    // Data source tracking
    string current_graph_file = "";
    string current_coord_file = "";
    string current_disruption_file = "";
    
public:
    DHLRoutingService();
    ~DHLRoutingService();
    
    // Main initialization
    bool initialize(const string& graph_file = "",
                   const string& coord_file = "",
                   const string& disruption_file = "");
    
    // Main routing function
    DHLRoutingResult findRoute(double start_lat, double start_lng, 
                              double dest_lat, double dest_lng,
                              bool use_disruptions = false,
                              double threshold_meters = 1000.0);
    
    // Utility functions
    bool isInitialized() const { return graph != nullptr && con_index != nullptr; }
    size_t getNodeCount() const { return graph ? graph->node_count() : 0; }
    size_t getEdgeCount() const { return graph ? graph->edge_count() : 0; }
    
    // Index statistics
    size_t getIndexSize() const { return con_index ? con_index->size() : 0; }
    size_t getIndexHeight() const { return con_index ? con_index->height() : 0; }
    double getAvgCutSize() const { return con_index ? con_index->avg_cut_size() : 0.0; }
    size_t getTotalLabels() const { return con_index ? con_index->label_count() : 0; }
    
    // Configuration
    void addBlockedNode(NodeID node);
    void removeBlockedNode(NodeID node);
    void clearBlockedNodes();
    bool isNodeBlocked(NodeID node) const;
};