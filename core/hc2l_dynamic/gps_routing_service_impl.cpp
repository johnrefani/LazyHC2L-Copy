#include <iostream>
#include <chrono>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "gps_routing_service.h"

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;
namespace fs = std::filesystem;

string GPSRoutingService::formatRouteTrace(const vector<unsigned int>& path) {
    if (path.empty()) return "";
    
    stringstream ss;
    if (path.size() <= 15) {
        for (size_t j = 0; j < path.size(); j++) {
            ss << path[j];
            if (j < path.size() - 1) ss << " → ";
        }
    } else {
        for (size_t j = 0; j < 5; j++) {
            ss << path[j] << " → ";
        }
        ss << "... → ";
        for (size_t j = path.size() - 3; j < path.size(); j++) {
            ss << path[j];
            if (j < path.size() - 1) ss << " → ";
        }
    }
    return ss.str();
}

GPSRoutingService::GPSRoutingService() : qc_router(nullptr), initialized(false), 
                                       labeling_size_mb(0.0), labeling_time_seconds(0.0), 
                                       labeling_metrics_computed(false) {}

GPSRoutingService::~GPSRoutingService() {
    if (qc_router) {
        delete qc_router;
    }
}

// Helper function to find the first existing file from a list of possible paths
string findExistingFile(const vector<string>& possible_paths, const string& file_type) {
    for (const string& path : possible_paths) {
        if (fs::exists(path)) {
            cerr << "✅ Found " << file_type << ": " << path << endl;
            return path;
        }
    }
    return "";
}

// Initialize the routing service
bool GPSRoutingService::initialize(const string& graph_file,
               const string& nodes_file,
               const string& disruptions_file) {
    try {
        // If specific files are provided, use them; otherwise search for files in common locations
        string actual_graph_file = graph_file;
        string actual_nodes_file = nodes_file;
        string actual_disruptions_file = disruptions_file;
        
        // If default paths are used, try to find files in multiple locations
        if (graph_file == "../../data/processed/qc_from_csv.gr") {
            // Possible graph file paths
            vector<string> graph_paths = {
                "data/processed/qc_from_csv.gr",
                "../data/processed/qc_from_csv.gr",
                "../../data/processed/qc_from_csv.gr",
            };
            
            string found_graph = findExistingFile(graph_paths, "graph file");
            if (!found_graph.empty()) {
                actual_graph_file = found_graph;
            }
        }
        
        if (nodes_file == "../../data/raw/quezon_city_nodes.csv") {
            // Possible coordinate file paths
            vector<string> coord_paths = {
                "data/raw/quezon_city_nodes.csv",
                "../data/raw/quezon_city_nodes.csv",
                "../../data/raw/quezon_city_nodes.csv",
            };
            
            string found_coords = findExistingFile(coord_paths, "coordinates file");
            if (!found_coords.empty()) {
                actual_nodes_file = found_coords;
            }
        }

        if (disruptions_file == "../../data/disruptions/qc_scenario_for_cpp_1.csv") {
            // Possible disruption file paths
            vector<string> disruption_paths = {
                "data/disruptions/qc_scenario_for_cpp_1.csv",
                "../data/disruptions/qc_scenario_for_cpp_1.csv",
                "../../data/disruptions/qc_scenario_for_cpp_1.csv",
            };
            
            string found_disruptions = findExistingFile(disruption_paths, "disruptions file");
            if (!found_disruptions.empty()) {
                actual_disruptions_file = found_disruptions;
            }
        }
        
        // Final validation that all required files exist
        vector<string> required_files = {actual_graph_file, actual_nodes_file, actual_disruptions_file};
        vector<string> missing_files;
        
        for (const string& file_path : required_files) {
            if (!fs::exists(file_path)) {
                missing_files.push_back(file_path);
            }
        }
        
        if (!missing_files.empty()) {
            cerr << "❌ GPSRoutingService initialization failed: Missing required files:" << endl;
            for (const string& missing_file : missing_files) {
                cerr << "   - " << missing_file << endl;
            }
            cerr << "Please ensure all data files are available before initialization." << endl;
            return false;
        }
        
        // Check if files are readable
        for (const string& file_path : required_files) {
            ifstream test_file(file_path);
            if (!test_file.is_open()) {
                cerr << "❌ GPSRoutingService initialization failed: Cannot read file '" 
                     << file_path << "'" << endl;
                return false;
            }
            test_file.close();
        }
        
        cerr << "✅ All required files validated successfully:" << endl;
        cerr << "   📊 Graph file: " << actual_graph_file << endl;
        cerr << "   📍 Nodes file: " << actual_nodes_file << endl;
        cerr << "   🔴 Disruptions file: " << actual_disruptions_file << endl;
        
        // Load graph
        ifstream gfs(actual_graph_file);
        if (!gfs.is_open()) {
            cerr << "❌ Failed to open graph file: " << actual_graph_file << endl;
            return false;
        }
        
        read_graph(graph, gfs);
        gfs.close();
        cerr << "✅ Graph loaded successfully" << endl;
        
        // Initialize labeling metrics to default values for now
        // We'll compute them separately to avoid graph corruption
        labeling_size_mb = 0.0;  // Will be computed on first query if needed
        labeling_time_seconds = 0.0;  // Will be computed on first query if needed
        labeling_metrics_computed = false;
        
        cerr << "📊 Graph ready for routing with " << graph.node_count() 
             << " nodes and " << graph.edge_count() << " edges" << endl;
        
        // Initialize HC2L with the loaded graph
        qc_router = new Dynamic(graph);
        cerr << "✅ HC2L Dynamic router initialized" << endl;
        
        // Initialize coordinate mapping for GPS-to-node conversion
        bool mapping_success = qc_router->initializeCoordinateMapping(actual_nodes_file, actual_disruptions_file);
        
        if (!mapping_success) {
            cerr << "❌ Failed to initialize coordinate mapping" << endl;
            delete qc_router;
            qc_router = nullptr;
            return false;
        }
        
        cerr << "✅ GPS coordinate mapping initialized successfully" << endl;
        
        initialized = true;
        cerr << "🎯 GPSRoutingService ready for GPS-based routing!" << endl;
        return true;
        
    } catch (const exception& e) {
        cerr << "❌ Exception during GPSRoutingService initialization: " << e.what() << endl;
        if (qc_router) {
            delete qc_router;
            qc_router = nullptr;
        }
        return false;
    }
}

// Main routing function
RoutingResult GPSRoutingService::findRoute(double start_latitude, double start_longitude,
                       double dest_latitude, double dest_longitude,
                       bool use_disrupted_mode) {
    RoutingResult result;
    result.success = false;
    result.had_disruptions = false;
    result.base_distance_meters = 0.0;
    result.distance_difference_meters = 0.0;
    result.distance_change_percentage = 0.0;
    
    if (!initialized || !qc_router) {
        result.error_message = "Service not initialized";
        return result;
    }
    
    try {
        // Set the appropriate mode
        if (use_disrupted_mode) {
            string disruptions_file = "../../data/disruptions/qc_scenario_for_cpp_1.csv";
            
            // Check if disruptions file exists before loading
            if (!fs::exists(disruptions_file)) {
                result.error_message = "Disruptions file not found: " + disruptions_file;
                return result;
            }
            
            // Check if disruptions file is readable
            ifstream test_disruptions(disruptions_file);
            if (!test_disruptions.is_open()) {
                result.error_message = "Cannot read disruptions file: " + disruptions_file;
                return result;
            }
            test_disruptions.close();
            
            qc_router->loadDisruptions(disruptions_file);
            // Let the Dynamic class automatically determine the appropriate mode based on impact score
            // Don't force DISRUPTED mode - let it choose between IMMEDIATE_UPDATE or LAZY_UPDATE
            
            // Get the mode that was automatically determined
            Mode current_mode = qc_router->getMode();
            
            // Convert mode enum to string for JSON response
            switch (current_mode) {
                case Mode::IMMEDIATE_UPDATE:
                    result.routing_mode = "IMMEDIATE_UPDATE";
                    cerr << "[GPS API] Mode determined: IMMEDIATE_UPDATE" << endl;
                    break;
                case Mode::LAZY_UPDATE:
                    result.routing_mode = "LAZY_UPDATE";
                    cerr << "[GPS API] Mode determined: LAZY_UPDATE" << endl;
                    break;
                case Mode::DISRUPTED:
                    result.routing_mode = "DISRUPTED";
                    cerr << "[GPS API] Mode determined: DISRUPTED" << endl;
                    break;
                case Mode::BASE:
                    result.routing_mode = "BASE";
                    cerr << "[GPS API] Mode determined: BASE" << endl;
                    break;
                default:
                    result.routing_mode = "UNKNOWN";
                    cerr << "[GPS API] Mode determined: UNKNOWN" << endl;
                    break;
            }
        } else {
            qc_router->setMode(Mode::BASE);
            result.routing_mode = "BASE";
        }
        
        // Perform GPS-based routing with real coordinate conversion
        auto routing_start = chrono::high_resolution_clock::now();
        auto route_info = qc_router->findRouteByGPS(start_latitude, start_longitude, dest_latitude, dest_longitude, true);
        auto routing_end = chrono::high_resolution_clock::now();
        auto routing_duration = chrono::duration_cast<chrono::microseconds>(routing_end - routing_start);
        
        if (route_info.path.empty()) {
            result.error_message = "No route found between the GPS coordinates";
            return result;
        }
        
        // Extract the actual nodes computed from GPS coordinates
        result.start_node = route_info.path.front();
        result.dest_node = route_info.path.back();
        
        // Fill in the result structure
        result.success = true;
        result.query_time_microseconds = routing_duration.count();
        result.total_distance_meters = route_info.total_distance;
        result.path = route_info.path;
        result.path_length = route_info.path.size();
        
        // Compute labeling metrics on first routing request (to avoid initialization overhead)
        if (!labeling_metrics_computed) {
            computeLabelingMetrics();
        }
        result.labeling_size_mb = labeling_size_mb;
        result.labeling_time_seconds = labeling_time_seconds;
        
        // GPS to Node conversion info
        stringstream gps_info;
        gps_info << "(" << fixed << setprecision(6) << start_latitude << "," << start_longitude 
                 << ") → Node " << result.start_node << " | (" 
                 << dest_latitude << "," << dest_longitude << ") → Node " << result.dest_node;
        result.gps_to_node_info = gps_info.str();
        
        // Complete route trace
        result.complete_route_trace = formatRouteTrace(result.path);
        
        // If we're in disrupted mode, also calculate the base route for comparison
        if (use_disrupted_mode) {
            result.had_disruptions = true;
            
            // Get base mode route for comparison
            qc_router->setMode(Mode::BASE);
            auto base_route_info = qc_router->findRouteByGPS(start_latitude, start_longitude, dest_latitude, dest_longitude, true);
            
            if (!base_route_info.path.empty()) {
                result.base_distance_meters = base_route_info.total_distance;
                result.distance_difference_meters = result.total_distance_meters - result.base_distance_meters;
                result.distance_change_percentage = (result.distance_difference_meters / result.base_distance_meters) * 100.0;
                
                // Generate route comparison message
                stringstream comparison;
                if (result.distance_difference_meters == 0) {
                    comparison << "Same route used (no impact from disruptions)";
                } else if (result.distance_difference_meters > 0) {
                    comparison << "Alternative route found (+" << fixed << setprecision(1) 
                              << result.distance_change_percentage << "% longer)";
                } else {
                    comparison << "Shorter route found (optimized)";
                }
                result.route_comparison = comparison.str();
            }
            
            // Reset back to disrupted mode
            qc_router->setMode(Mode::DISRUPTED);
        }
        
    } catch (const exception& e) {
        result.error_message = string("Exception: ") + e.what();
        return result;
    }
    
    return result;
}

GPSRoutingService::NetworkStats GPSRoutingService::getNetworkStats() {
    return NetworkStats();
}

bool GPSRoutingService::isInitialized() const {
    return initialized;
}

// Compute labeling metrics on demand
void GPSRoutingService::computeLabelingMetrics() {
    if (labeling_metrics_computed || !initialized) {
        return;  // Already computed or not initialized
    }
    
    try {
        cerr << "🔧 Computing labeling metrics (on-demand)..." << endl;
        auto labeling_start = chrono::high_resolution_clock::now();
        
        // Read the graph again to get a fresh copy for ContractionIndex building
        // This ensures we don't modify the original graph used for routing
        Graph temp_graph;
        
        // Find and read the graph file again
        vector<string> graph_paths = {
            "data/processed/qc_from_csv.gr",
            "../data/processed/qc_from_csv.gr", 
            "../../data/processed/qc_from_csv.gr",
        };
        
        string graph_file;
        for (const string& path : graph_paths) {
            if (fs::exists(path)) {
                graph_file = path;
                break;
            }
        }
        
        if (graph_file.empty()) {
            cerr << "❌ Could not find graph file for labeling metrics computation" << endl;
            return;
        }
        
        ifstream gfs(graph_file);
        if (!gfs.is_open()) {
            cerr << "❌ Failed to open graph file for labeling metrics: " << graph_file << endl;
            return;
        }
        
        read_graph(temp_graph, gfs);
        gfs.close();
        
        // Build ContractionIndex on the temporary graph
        vector<CutIndex> ci;
        size_t num_shortcuts = temp_graph.create_cut_index(ci, 0.5);
        
        auto labeling_end = chrono::high_resolution_clock::now();
        labeling_time_seconds = chrono::duration<double>(labeling_end - labeling_start).count();
        
        // Create ContractionIndex to get size
        ContractionIndex contraction_index(ci);
        labeling_size_mb = contraction_index.size() / (1024.0 * 1024.0);
        
        labeling_metrics_computed = true;
        
        cerr << "✅ Labeling metrics computed - Size: " << fixed << setprecision(2) 
             << labeling_size_mb << " MB, Time: " << labeling_time_seconds << " s" << endl;
             
    } catch (const exception& e) {
        cerr << "❌ Error computing labeling metrics: " << e.what() << endl;
        labeling_size_mb = 0.0;
        labeling_time_seconds = 0.0;
    }
}