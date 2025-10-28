#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include "gps_routing_service.h"
#include "include/road_network.h"

using namespace std;
namespace fs = std::filesystem;

// Function to check if required files exist
struct FileValidationResult {
    bool all_files_exist;
    vector<string> missing_files;
    string error_message;
    string found_graph_file;
    string found_nodes_file;
    string found_disruptions_file;
};

// Helper function to find the first existing file from a list of possible paths
string findExistingFile(const vector<string>& possible_paths) {
    for (const string& path : possible_paths) {
        if (fs::exists(path)) {
            return path;
        }
    }
    return "";
}

FileValidationResult validateRequiredFiles() {
    FileValidationResult result;
    result.all_files_exist = true;
    
    // Possible graph file paths
    vector<string> graph_paths = {
        "../../data/processed/qc_from_csv.gr",      
        "data/processed/qc_from_csv.gr",
        "../data/processed/qc_from_csv.gr",
    };
    
    // Possible coordinate file paths
    vector<string> coord_paths = {
        "../../data/raw/quezon_city_nodes.csv", 
        "data/raw/quezon_city_nodes.csv",
        "../data/raw/quezon_city_nodes.csv",
    };
    
    // Possible disruption file paths
    vector<string> disruption_paths = {
        "../../data/disruptions/qc_scenario_for_cpp_1.csv",
        "data/disruptions/qc_scenario_for_cpp_1.csv",
        "../data/disruptions/qc_scenario_for_cpp_1.csv",
    };
    
    // Find existing files
    result.found_graph_file = findExistingFile(graph_paths);
    result.found_nodes_file = findExistingFile(coord_paths);
    result.found_disruptions_file = findExistingFile(disruption_paths);
    
    // Check if all files were found
    if (result.found_graph_file.empty()) {
        result.all_files_exist = false;
        result.missing_files.push_back("Graph file (qc_from_csv.gr)");
    }
    if (result.found_nodes_file.empty()) {
        result.all_files_exist = false;
        result.missing_files.push_back("Nodes file (quezon_city_nodes.csv)");
    }
    if (result.found_disruptions_file.empty()) {
        result.all_files_exist = false;
        result.missing_files.push_back("Disruptions file (qc_scenario_for_cpp_1.csv)");
    }
    
    // Generate error message if files are missing
    if (!result.all_files_exist) {
        stringstream error_msg;
        error_msg << "Missing required files for GPS routing service: ";
        for (size_t i = 0; i < result.missing_files.size(); i++) {
            error_msg << "'" << result.missing_files[i] << "'";
            if (i < result.missing_files.size() - 1) {
                error_msg << ", ";
            }
        }
        error_msg << ". Please ensure all data files are available in one of the expected locations.";
        result.error_message = error_msg.str();
    }
    
    return result;
}

// Function to convert a path to JSON coordinate array
string pathToJsonCoordinates(const GPSRoutingService& router, const vector<unsigned int>& path) {
    stringstream json;
    json << "[";
    
    // For simplicity, we'll create coordinates from the path nodes
    // In a real implementation, you'd want to get actual coordinates for each node
    for (size_t i = 0; i < path.size(); i++) {
        // For now, just output the node IDs as placeholder coordinates
        // You would need to implement coordinate lookup for each node
        json << "{\"node_id\": " << path[i] << ", \"lat\": 0, \"lng\": 0}";
        if (i < path.size() - 1) json << ", ";
    }
    
    json << "]";
    return json.str();
}

// Function to escape JSON strings
string escapeJsonString(const string& input) {
    stringstream escaped;
    for (char c : input) {
        switch (c) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default: escaped << c; break;
        }
    }
    return escaped.str();
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc != 7) {
        cout << "{\"success\": false, \"error\": \"Usage: " << argv[0] 
             << " <start_lat> <start_lng> <dest_lat> <dest_lng> <use_disruptions> <tau_threshold>\"}";
        return 1;
    }
    
    try {
        // First, validate that all required files exist
        auto file_validation = validateRequiredFiles();
        if (!file_validation.all_files_exist) {
            cout << "{\"success\": false, \"error\": \"" << escapeJsonString(file_validation.error_message) << "\"}";
            return 1;
        }
        
        // Parse arguments
        double start_lat = stod(argv[1]);
        double start_lng = stod(argv[2]);
        double dest_lat = stod(argv[3]);
        double dest_lng = stod(argv[4]);
        bool use_disruptions = (string(argv[5]) == "true" || string(argv[5]) == "1");
        double tau_threshold = stod(argv[6]);
        
        // Validate tau_threshold range
        if (tau_threshold < 0.1 || tau_threshold > 1.0) {
            cout << "{\"success\": false, \"error\": \"tau_threshold must be between 0.1 and 1.0 (inclusive). Provided: " << tau_threshold << "\"}";
            return 1;
        }
        
        // Set the global threshold for HC2L Dynamic
        road_network::DISRUPTION_THRESHOLD_TAU = tau_threshold;
        
        // Initialize GPS routing service (suppress output by redirecting to stderr)
        GPSRoutingService router;
        if (!router.initialize(file_validation.found_graph_file, 
                             file_validation.found_nodes_file, 
                             file_validation.found_disruptions_file)) {
            cout << "{\"success\": false, \"error\": \"Failed to initialize GPS routing service\"}";
            return 1;
        }
        
        // Compute route
        auto result = router.findRoute(start_lat, start_lng, dest_lat, dest_lng, use_disruptions);
        
        if (!result.success) {
            cout << "{\"success\": false, \"error\": \"" << escapeJsonString(result.error_message) << "\"}";
            return 1;
        }
        
        // Build JSON response
        stringstream json_response;
        json_response << fixed << setprecision(6);
        
        json_response << "{";
        json_response << "\"success\": true,";
        
        // Include mode information in algorithm name for clarity
        string algorithm_name = "D-HC2L";
        if (use_disruptions) {
            algorithm_name += " (" + result.routing_mode + ")";
        } else {
            algorithm_name += " (BASE)";
        }
        json_response << "\"algorithm\": \"" << algorithm_name << "\",";
        json_response << "\"algorithm_base\": \"D-HC2L\",";
        
        // Route metrics
        json_response << "\"metrics\": {";
        json_response << "\"query_time_microseconds\": " << result.query_time_microseconds << ",";
        json_response << "\"query_time_ms\": " << (result.query_time_microseconds / 1000.0) << ",";
        json_response << "\"total_distance_meters\": " << result.total_distance_meters << ",";
        json_response << "\"path_length\": " << result.path_length << ",";
        json_response << "\"routing_mode\": \"" << result.routing_mode << "\",";
        json_response << "\"uses_disruptions\": " << (use_disruptions ? "true" : "false") << ",";
        json_response << "\"tau_threshold\": " << tau_threshold << ",";
        
        // Add mode-specific information
        if (result.routing_mode == "IMMEDIATE_UPDATE") {
            json_response << "\"update_strategy\": \"immediate\",";
            json_response << "\"mode_explanation\": \"High-impact disruptions detected. Labels precomputed and kept fresh in background.\",";
            json_response << "\"labels_status\": \"precomputed_fresh\",";
        } else if (result.routing_mode == "LAZY_UPDATE") {
            json_response << "\"update_strategy\": \"lazy\",";
            json_response << "\"mode_explanation\": \"Low-impact disruptions detected. Labels repaired on-demand during queries.\",";
            json_response << "\"labels_status\": \"on_demand_repair\",";
        } else if (result.routing_mode == "DISRUPTED") {
            json_response << "\"update_strategy\": \"standard\",";
            json_response << "\"mode_explanation\": \"Standard disrupted mode without dynamic update optimization.\",";
            json_response << "\"labels_status\": \"standard\",";
        } else if (result.routing_mode == "BASE") {
            json_response << "\"update_strategy\": \"none\",";
            json_response << "\"mode_explanation\": \"Base mode without disruptions.\",";
            json_response << "\"labels_status\": \"original\",";
        }
        
        // Labeling metrics
        json_response << "\"labeling_size_mb\": " << result.labeling_size_mb << ",";
        json_response << "\"labeling_time_seconds\": " << result.labeling_time_seconds;
        
        // Add disruption comparison data if available
        if (result.had_disruptions) {
            json_response << ",";
            json_response << "\"base_distance_meters\": " << result.base_distance_meters << ",";
            json_response << "\"distance_difference_meters\": " << result.distance_difference_meters << ",";
            json_response << "\"distance_change_percentage\": " << result.distance_change_percentage << ",";
            json_response << "\"route_comparison\": \"" << escapeJsonString(result.route_comparison) << "\"";
        }
        
        json_response << "},";
        
        // GPS to Node mapping
        json_response << "\"gps_mapping\": {";
        json_response << "\"start_node\": " << result.start_node << ",";
        json_response << "\"dest_node\": " << result.dest_node << ",";
        json_response << "\"gps_to_node_info\": \"" << escapeJsonString(result.gps_to_node_info) << "\"";
        json_response << "},";
        
        // Route data
        json_response << "\"route\": {";
        json_response << "\"complete_trace\": \"" << escapeJsonString(result.complete_route_trace) << "\",";
        
        // Path nodes
        json_response << "\"path_nodes\": [";
        for (size_t i = 0; i < result.path.size(); i++) {
            json_response << result.path[i];
            if (i < result.path.size() - 1) json_response << ", ";
        }
        json_response << "],";
        
        // Coordinates (simplified - in a real implementation you'd get actual coordinates)
        json_response << "\"coordinates\": [";
        json_response << "{\"lat\": " << start_lat << ", \"lng\": " << start_lng << ", \"node_id\": " << result.start_node << "},";
        json_response << "{\"lat\": " << dest_lat << ", \"lng\": " << dest_lng << ", \"node_id\": " << result.dest_node << "}";
        json_response << "]";
        
        json_response << "},";
        
        // Input coordinates
        json_response << "\"input\": {";
        json_response << "\"start_lat\": " << start_lat << ",";
        json_response << "\"start_lng\": " << start_lng << ",";
        json_response << "\"dest_lat\": " << dest_lat << ",";
        json_response << "\"dest_lng\": " << dest_lng << ",";
        json_response << "\"use_disruptions\": " << (use_disruptions ? "true" : "false") << ",";
        json_response << "\"tau_threshold\": " << tau_threshold;
        json_response << "}";
        
        json_response << "}";
        
        cout << json_response.str();
        
    } catch (const exception& e) {
        cout << "{\"success\": false, \"error\": \"Exception: " << escapeJsonString(e.what()) << "\"}";
        return 1;
    }
    
    return 0;
}