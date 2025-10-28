#include "dhl_routing_service.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sys/stat.h>

using namespace std;

DHLRoutingService::DHLRoutingService() : coordinate_mapping_initialized(false) {
    graph = nullptr;
    con_index = nullptr;
    ch = nullptr;
}

DHLRoutingService::~DHLRoutingService() {
    // Smart pointers will automatically clean up
}

// Helper method to check if file exists
bool DHLRoutingService::file_exists(const string& filepath) const {
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

// Find data files with fallback paths
bool DHLRoutingService::find_data_files(string& graph_file, string& coord_file, string& disruption_file) const {
    // Possible graph file paths
    vector<string> graph_paths = {
        "data/processed/qc_from_csv.gr",
        "../data/processed/qc_from_csv.gr",
        "../../data/processed/qc_from_csv.gr",
    };
    
    // Possible coordinate file paths
    vector<string> coord_paths = {
        "data/raw/quezon_city_nodes.csv",
        "../data/raw/quezon_city_nodes.csv",
        "../../data/raw/quezon_city_nodes.csv",
    };
    
    // Possible disruption file paths
    vector<string> disruption_paths = {
        "data/disruptions/qc_scenario_for_cpp_1.csv",
        "../data/disruptions/qc_scenario_for_cpp_1.csv",
        "../../data/disruptions/qc_scenario_for_cpp_1.csv",

    };
    
    // Find graph file
    bool found_graph = false;
    for (const auto& path : graph_paths) {
        if (file_exists(path)) {
            graph_file = path;
            found_graph = true;
            break;
        }
    }
    
    // Find coordinate file
    bool found_coord = false;
    for (const auto& path : coord_paths) {
        if (file_exists(path)) {
            coord_file = path;
            found_coord = true;
            break;
        }
    }
    
    // Find disruption file (optional)
    if (!disruption_file.empty()) {
        bool found_disruption = false;
        for (const auto& path : disruption_paths) {
            if (file_exists(path)) {
                disruption_file = path;
                found_disruption = true;
                break;
            }
        }
        // If not found, clear it (optional file)
        if (!found_disruption) {
            disruption_file = "";
        }
    }
    
    return found_graph && found_coord;
}

bool DHLRoutingService::load_graph(const string& graph_file) {
    ifstream ifs(graph_file);
    if (!ifs.is_open()) {
        cerr << "Error: Cannot open graph file: " << graph_file << endl;
        return false;
    }
    
    graph = make_unique<Graph>();
    read_graph(*graph, ifs);
    ifs.close();
    
    return true;
}

bool DHLRoutingService::build_index() {
    if (!graph) return false;
    
    auto start_time = chrono::high_resolution_clock::now();
    
    // Degree 1 node contraction
    vector<Neighbor> closest;
    graph->contract(closest);
    
    // Construct DHL index
    vector<CutIndex> ci;
    graph->create_cut_index(ci, 0.2); // 0.2 = balance parameter
    graph->reset();
    
    // Create contraction hierarchy and index
    ch = make_unique<ContractionHierarchy>();
    graph->create_contraction_hierarchy(*ch, ci, closest);
    con_index = make_unique<ContractionIndex>(ci, closest);
    
    auto end_time = chrono::high_resolution_clock::now();
    last_labeling_time_ms = chrono::duration<double, milli>(end_time - start_time).count();
    last_labeling_size_bytes = con_index->size();
    
    return true;
}

NodeID DHLRoutingService::find_nearest_node(double lat, double lng, double threshold_meters) const {
    if (!coordinate_mapping_initialized) {
        return 0;
    }
    
    double distance;
    NodeID node = coordinate_mapper.findNearestNode(lat, lng, distance);
    
    // Check if within threshold
    if (distance > threshold_meters) {
        return 0;
    }
    
    return node;
}

vector<NodeID> DHLRoutingService::reconstruct_path(NodeID start, NodeID dest) {
    // For DHL, we need to run path reconstruction using Dijkstra on the original graph
    // since DHL only provides distance queries, not path reconstruction
    
    vector<NodeID> path;
    
    if (!graph || start == 0 || dest == 0) {
        if (start != 0) path.push_back(start);
        if (dest != 0 && start != dest) path.push_back(dest);
        return path;
    }
    
    if (start == dest) {
        path.push_back(start);
        return path;
    }
    
    try {
        // Run custom Dijkstra with parent tracking for path reconstruction
        path = dijkstra_with_path_reconstruction(start, dest);
        
        // If path reconstruction failed, fall back to simple start->dest
        if (path.empty()) {
            path.push_back(start);
            path.push_back(dest);
        }
        
    } catch (const exception& e) {
        cerr << "Path reconstruction failed: " << e.what() << endl;
        // Fall back to simple path
        path.push_back(start);
        path.push_back(dest);
    }
    
    return path;
}

vector<NodeID> DHLRoutingService::dijkstra_with_path_reconstruction(NodeID start, NodeID dest) {
    if (!graph || start == dest) {
        vector<NodeID> path;
        if (start != 0) path.push_back(start);
        return path;
    }
    
    // Custom Dijkstra implementation with path reconstruction
    map<NodeID, distance_t> distances;
    map<NodeID, NodeID> parents;
    set<NodeID> visited;
    priority_queue<pair<distance_t, NodeID>, vector<pair<distance_t, NodeID>>, greater<pair<distance_t, NodeID>>> pq;
    
    // Initialize
    distances[start] = 0;
    pq.push({0, start});
    
    while (!pq.empty()) {
        auto [current_dist, current_node] = pq.top();
        pq.pop();
        
        if (visited.count(current_node)) continue;
        visited.insert(current_node);
        
        // If we reached the destination, reconstruct path
        if (current_node == dest) {
            vector<NodeID> path;
            NodeID node = dest;
            while (node != start) {
                path.push_back(node);
                if (parents.find(node) == parents.end()) {
                    // Path reconstruction failed
                    return {};
                }
                node = parents[node];
            }
            path.push_back(start);
            reverse(path.begin(), path.end());
            return path;
        }
        
        // Explore neighbors using the new get_neighbors method
        try {
            const auto& neighbors = graph->get_neighbors(current_node);
            for (const auto& neighbor : neighbors) {
                NodeID neighbor_id = neighbor.node;
                distance_t edge_weight = neighbor.distance;
                
                if (visited.count(neighbor_id)) continue;
                
                // Check if this edge is disrupted when disruptions are enabled
                if (!disrupted_edges.empty()) {
                    string edge_key = to_string(min(current_node, neighbor_id)) + "_" + to_string(max(current_node, neighbor_id));
                    if (disrupted_edges.find(edge_key) != disrupted_edges.end()) {
                        continue; // Skip this edge - it's disrupted
                    }
                }
                
                // Check if the neighbor node is blocked
                if (isNodeBlocked(neighbor_id)) {
                    continue; // Skip this neighbor - it's blocked
                }
                
                distance_t new_dist = current_dist + edge_weight;
                
                if (distances.find(neighbor_id) == distances.end() || new_dist < distances[neighbor_id]) {
                    distances[neighbor_id] = new_dist;
                    parents[neighbor_id] = current_node;
                    pq.push({new_dist, neighbor_id});
                }
            }
        } catch (const exception& e) {
            cerr << "Error accessing neighbors for node " << current_node << ": " << e.what() << endl;
            break;
        }
    }
    
    // If we couldn't find a path, return empty
    return {};
}

string DHLRoutingService::create_route_trace(const vector<NodeID>& path) const {
    if (path.empty()) return "No path found";
    
    stringstream trace;
    trace << "DHL Route (";
    for (size_t i = 0; i < path.size(); i++) {
        if (i > 0) trace << " -> ";
        trace << path[i];
        
        // Add coordinates if available
        double lat, lng;
        if (coordinate_mapper.getNodeCoordinates(path[i], lat, lng)) {
            trace << " (" << fixed << setprecision(6) 
                  << lat << ", " << lng << ")";
        }
    }
    trace << ")";
    return trace.str();
}

bool DHLRoutingService::initialize(const string& graph_file, const string& coord_file, const string& disruption_file) {
    try {
        // Use provided files or detect automatically
        string final_graph_file = graph_file;
        string final_coord_file = coord_file;
        string final_disruption_file = disruption_file;
        
        // If no files provided, try to detect them
        if (graph_file.empty() || coord_file.empty()) {
            if (!find_data_files(final_graph_file, final_coord_file, final_disruption_file)) {
                cerr << "Error: Could not find required data files automatically" << endl;
                return false;
            }
        }
        
        // Load graph
        if (!load_graph(final_graph_file)) {
            return false;
        }
        current_graph_file = final_graph_file;
        
        // Load coordinates using coordinate mapper
        if (!coordinate_mapper.loadNodeCoordinates(final_coord_file)) {
            return false;
        }
        current_coord_file = final_coord_file;
        coordinate_mapping_initialized = true;
        
        // Load disruptions (optional) using coordinate mapper
        if (!final_disruption_file.empty()) {
            coordinate_mapper.loadRoadSegments(final_disruption_file);
            current_disruption_file = final_disruption_file;
        }
        
        // Build DHL index
        if (!build_index()) {
            return false;
        }
        
        cerr << "DHL routing service initialized successfully!" << endl;
        cerr << "Graph: " << current_graph_file << endl;
        cerr << "Coordinates: " << current_coord_file << endl;
        if (!current_disruption_file.empty()) {
            cerr << "Disruptions: " << current_disruption_file << endl;
        }
        
        return true;
        
    } catch (const exception& e) {
        cerr << "Error during initialization: " << e.what() << endl;
        return false;
    }
}

DHLRoutingResult DHLRoutingService::findRoute(double start_lat, double start_lng, 
                                             double dest_lat, double dest_lng,
                                             bool use_disruptions, double threshold_meters) {
    DHLRoutingResult result;
    
    if (!isInitialized()) {
        result.error_message = "DHL routing service not initialized";
        return result;
    }
    
    // Store input parameters
    result.start_lat = start_lat;
    result.start_lng = start_lng;
    result.dest_lat = dest_lat;
    result.dest_lng = dest_lng;
    result.uses_disruptions = use_disruptions;
    result.coordinate_threshold_meters = threshold_meters;
    
    // Find nearest nodes
    NodeID start_node = find_nearest_node(start_lat, start_lng, threshold_meters);
    NodeID dest_node = find_nearest_node(dest_lat, dest_lng, threshold_meters);
    
    if (start_node == 0) {
        result.error_message = "No start node found within " + to_string(threshold_meters) + "m threshold";
        return result;
    }
    
    if (dest_node == 0) {
        result.error_message = "No destination node found within " + to_string(threshold_meters) + "m threshold";
        return result;
    }
    
    result.start_node = start_node;
    result.dest_node = dest_node;
    
    // Create GPS mapping info
    double start_coord_lat, start_coord_lng;
    double dest_coord_lat, dest_coord_lng;
    bool start_coord_found = coordinate_mapper.getNodeCoordinates(start_node, start_coord_lat, start_coord_lng);
    bool dest_coord_found = coordinate_mapper.getNodeCoordinates(dest_node, dest_coord_lat, dest_coord_lng);
    
    stringstream gps_info;
    gps_info << "Start: (" << start_lat << ", " << start_lng << ") -> Node " << start_node;
    if (start_coord_found) {
        gps_info << " at (" << start_coord_lat << ", " << start_coord_lng << ")";
    }
    gps_info << "; Dest: (" << dest_lat << ", " << dest_lng << ") -> Node " << dest_node;
    if (dest_coord_found) {
        gps_info << " at (" << dest_coord_lat << ", " << dest_coord_lng << ")";
    }
    result.gps_to_node_info = gps_info.str();
    
    // Check for blocked nodes
    if (use_disruptions) {
        if (isNodeBlocked(start_node)) {
            result.error_message = "Start node " + to_string(start_node) + " is blocked";
            return result;
        }
        if (isNodeBlocked(dest_node)) {
            result.error_message = "Destination node " + to_string(dest_node) + " is blocked";
            return result;
        }
    }
    
    // Perform DHL query
    auto query_start = chrono::high_resolution_clock::now();
    distance_t distance;
    size_t hoplinks;
    
    if (use_disruptions && !disrupted_edges.empty()) {
        // When disruptions are present, use Dijkstra for accurate distance calculation
        // that respects blocked edges
        result.path = dijkstra_with_path_reconstruction(start_node, dest_node);
        if (result.path.empty()) {
            distance = infinity;
            hoplinks = 0;
        } else {
            // Calculate distance from path
            distance = 0;
            for (size_t i = 0; i < result.path.size() - 1; i++) {
                NodeID from = result.path[i];
                NodeID to = result.path[i + 1];
                // Find edge weight
                try {
                    const auto& neighbors = graph->get_neighbors(from);
                    for (const auto& neighbor : neighbors) {
                        if (neighbor.node == to) {
                            distance += neighbor.distance;
                            break;
                        }
                    }
                } catch (const exception& e) {
                    // If we can't find the edge weight, estimate
                    distance += 1;
                }
            }
            hoplinks = result.path.size() - 1; // Number of edges traversed
        }
    } else {
        // Use precomputed DHL index when no disruptions
        distance = con_index->get_distance(start_node, dest_node);
        hoplinks = con_index->get_hoplinks(start_node, dest_node);
    }
    
    auto query_end = chrono::high_resolution_clock::now();
    
    result.query_time_microseconds = chrono::duration<double, micro>(query_end - query_start).count();
    
    if (distance == infinity) {
        result.error_message = "No path exists between nodes " + to_string(start_node) + " and " + to_string(dest_node);
        return result;
    }
    
    // Fill in results
    result.success = true;
    result.total_distance = distance;
    result.hoplinks_examined = hoplinks;
    result.labeling_time_ms = last_labeling_time_ms;
    result.labeling_size_bytes = last_labeling_size_bytes;
    
    // Data sources
    result.data_sources.graph_file = current_graph_file;
    result.data_sources.coordinates_file = current_coord_file;
    result.data_sources.disruptions_file = current_disruption_file;
    
    // Index statistics
    result.index_height = con_index->height();
    result.avg_cut_size = con_index->avg_cut_size();
    result.total_labels = con_index->label_count();
    
    // Reconstruct path if not already done
    if (result.path.empty()) {
        result.path = reconstruct_path(start_node, dest_node);
    }
    result.path_length = result.path.size();
    
    // Create route trace
    result.complete_route_trace = create_route_trace(result.path);
    
    // Disruption information
    if (use_disruptions) {
        for (const auto& edge : disrupted_edges) {
            result.blocked_edges.push_back(edge.first);
        }
        for (NodeID node : blocked_nodes) {
            result.blocked_nodes.push_back(node);
        }
    }
    
    return result;
}

void DHLRoutingService::addBlockedNode(NodeID node) {
    blocked_nodes.insert(node);
}

void DHLRoutingService::removeBlockedNode(NodeID node) {
    blocked_nodes.erase(node);
}

void DHLRoutingService::clearBlockedNodes() {
    blocked_nodes.clear();
}

bool DHLRoutingService::isNodeBlocked(NodeID node) const {
    return blocked_nodes.find(node) != blocked_nodes.end();
}