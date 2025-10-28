#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include "dhl_routing_service.h"

using namespace std;

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

// Function to convert vector to JSON array
template<typename T>
string vectorToJson(const vector<T>& vec) {
    stringstream json;
    json << "[";
    for (size_t i = 0; i < vec.size(); i++) {
        json << vec[i];
        if (i < vec.size() - 1) json << ", ";
    }
    json << "]";
    return json.str();
}

// Function to convert string vector to JSON array
string stringVectorToJson(const vector<string>& vec) {
    stringstream json;
    json << "[";
    for (size_t i = 0; i < vec.size(); i++) {
        json << "\"" << escapeJsonString(vec[i]) << "\"";
        if (i < vec.size() - 1) json << ", ";
    }
    json << "]";
    return json.str();
}

int main(int argc, char* argv[]) {
    // Force immediate output flushing for Windows compatibility
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);
    
    // Check arguments
    if (argc != 6) {
        cout << "{\"success\": false, \"error\": \"Usage: " << argv[0] 
             << " <start_lat> <start_lng> <dest_lat> <dest_lng> <use_disruptions>\"}" << endl;
        cout.flush();
        return 1;
    }
    
    try {
        // Parse arguments
        double start_lat = stod(argv[1]);
        double start_lng = stod(argv[2]);
        double dest_lat = stod(argv[3]);
        double dest_lng = stod(argv[4]);
        bool use_disruptions = (string(argv[5]) == "true" || string(argv[5]) == "1");
        
        // Initialize DHL routing service
        DHLRoutingService dhl_service;
        
        // Try different data paths - expanded for Windows compatibility
        vector<string> possible_paths = {
            "data/processed/qc_from_csv.gr",                    // Next to exe
            "../data/processed/qc_from_csv.gr",                 // One level up
            "../../data/processed/qc_from_csv.gr",              // Two levels up
            "LazyHC2L/data/processed/qc_from_csv.gr",           // In LazyHC2L folder
            "../LazyHC2L/data/processed/qc_from_csv.gr",        // LazyHC2L one level up
            "./data/processed/qc_from_csv.gr"                   // Current directory
        };
        
        vector<string> coord_paths = {
            "data/raw/quezon_city_nodes.csv",
            "../data/raw/quezon_city_nodes.csv", 
            "../../data/raw/quezon_city_nodes.csv",
            "LazyHC2L/data/raw/quezon_city_nodes.csv",
            "../LazyHC2L/data/raw/quezon_city_nodes.csv",
            "./data/raw/quezon_city_nodes.csv"
        };
        
        vector<string> disruption_paths = {
            "data/disruptions/qc_scenario_for_cpp_1.csv",
            "../data/disruptions/qc_scenario_for_cpp_1.csv",
            "../../data/disruptions/qc_scenario_for_cpp_1.csv",
            "LazyHC2L/data/disruptions/qc_scenario_for_cpp_1.csv",
            "../LazyHC2L/data/disruptions/qc_scenario_for_cpp_1.csv",
            "./data/disruptions/qc_scenario_for_cpp_1.csv"
        };
        
        bool initialized = false;
        string graph_path, coord_path, disruption_path;
        
        // Try to find working paths
        for (size_t i = 0; i < possible_paths.size() && !initialized; i++) {
            for (size_t j = 0; j < coord_paths.size() && !initialized; j++) {
                string disr_path = use_disruptions ? disruption_paths[min(i, disruption_paths.size()-1)] : "";
                
                if (dhl_service.initialize(possible_paths[i], coord_paths[j], disr_path)) {
                    initialized = true;
                    graph_path = possible_paths[i];
                    coord_path = coord_paths[j];
                    disruption_path = disr_path;
                }
            }
        }
        
        if (!initialized) {
            cout << "{\"success\": false, \"error\": \"Failed to initialize DHL routing service. Check data file paths.\"}" << endl;
            cout.flush();
            return 1;
        }
        
        // Compute route
        auto result = dhl_service.findRoute(start_lat, start_lng, dest_lat, dest_lng, use_disruptions);
        
        if (!result.success) {
            cout << "{\"success\": false, \"error\": \"" << escapeJsonString(result.error_message) << "\"}" << endl;
            cout.flush();
            return 1;
        }
        
        // Build JSON response
        stringstream json_response;
        json_response << fixed << setprecision(6);
        
        json_response << "{";
        json_response << "\"success\": true,";
        json_response << "\"algorithm\": \"DHL (Dual-Hierarchy Labelling)\",";
        
        // Performance metrics
        json_response << "\"metrics\": {";
        json_response << "\"query_time_microseconds\": " << result.query_time_microseconds << ",";
        json_response << "\"query_time_ms\": " << (result.query_time_microseconds / 1000.0) << ",";
        json_response << "\"labeling_time_ms\": " << result.labeling_time_ms << ",";
        json_response << "\"labeling_size_bytes\": " << result.labeling_size_bytes << ",";
        json_response << "\"total_distance_units\": " << result.total_distance << ",";
        json_response << "\"path_length\": " << result.path_length << ",";
        json_response << "\"hoplinks_examined\": " << result.hoplinks_examined << ",";
        json_response << "\"routing_mode\": \"" << result.routing_mode << "\",";
        json_response << "\"uses_disruptions\": " << (result.uses_disruptions ? "true" : "false");
        json_response << "},";
        
        // Index statistics
        json_response << "\"index_stats\": {";
        json_response << "\"index_height\": " << result.index_height << ",";
        json_response << "\"avg_cut_size\": " << result.avg_cut_size << ",";
        json_response << "\"total_labels\": " << result.total_labels << ",";
        json_response << "\"graph_nodes\": " << dhl_service.getNodeCount() << ",";
        json_response << "\"graph_edges\": " << dhl_service.getEdgeCount();
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
        json_response << "\"path_nodes\": " << vectorToJson(result.path) << ",";
        
        // Coordinates
        json_response << "\"coordinates\": [";
        json_response << "{\"type\": \"start\", \"lat\": " << start_lat << ", \"lng\": " << start_lng << ", \"node_id\": " << result.start_node << "},";
        json_response << "{\"type\": \"destination\", \"lat\": " << dest_lat << ", \"lng\": " << dest_lng << ", \"node_id\": " << result.dest_node << "}";
        json_response << "]";
        json_response << "},";
        
        // Disruption information
        json_response << "\"disruptions\": {";
        json_response << "\"enabled\": " << (result.uses_disruptions ? "true" : "false") << ",";
        json_response << "\"blocked_edges\": " << stringVectorToJson(result.blocked_edges) << ",";
        json_response << "\"blocked_nodes\": " << vectorToJson(result.blocked_nodes);
        json_response << "},";
        
        // Data sources
        json_response << "\"data_sources\": {";
        json_response << "\"graph_file\": \"" << escapeJsonString(graph_path) << "\",";
        json_response << "\"coordinates_file\": \"" << escapeJsonString(coord_path) << "\",";
        json_response << "\"disruptions_file\": \"" << escapeJsonString(disruption_path) << "\"";
        json_response << "},";
        
        // Input parameters
        json_response << "\"input\": {";
        json_response << "\"start_lat\": " << start_lat << ",";
        json_response << "\"start_lng\": " << start_lng << ",";
        json_response << "\"dest_lat\": " << dest_lat << ",";
        json_response << "\"dest_lng\": " << dest_lng << ",";
        json_response << "\"use_disruptions\": " << (use_disruptions ? "true" : "false");
        json_response << "}";
        
        json_response << "}";
        
        cout << json_response.str() << endl;
        cout.flush();
        
    } catch (const exception& e) {
        cout << "{\"success\": false, \"error\": \"Exception: " << escapeJsonString(e.what()) << "\"}" << endl;
        cout.flush();
        return 1;
    }
    
    return 0;
}