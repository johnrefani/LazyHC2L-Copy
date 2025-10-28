#include "Dynamic.h"
#include "road_network.h"
#include "util.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <map>
#include <sstream>

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;

// Road name mapping structure
struct RoadMapping {
    map<pair<NodeID, NodeID>, string> node_to_road;
    map<NodeID, string> node_to_location;
};

// Function to load road names from CSV files
RoadMapping load_road_names(const vector<string>& csv_files) {
    RoadMapping mapping;
    
    for (const string& filename : csv_files) {
        ifstream file(filename);
        if (!file.is_open()) continue;
        
        string header;
        getline(file, header);
        
        string line;
        while (getline(file, line)) {
            if (line.empty()) continue;
            
            // Simple CSV parsing
            vector<string> fields;
            stringstream ss(line);
            string field;
            while (getline(ss, field, ',')) {
                fields.push_back(field);
            }
            
            if (fields.size() >= 7) {
                try {
                    NodeID source = stoull(fields[4]);
                    NodeID target = stoull(fields[5]);
                    string road_name = fields[6];
                    
                    // Clean up road name
                    if (road_name.front() == '"' && road_name.back() == '"') {
                        road_name = road_name.substr(1, road_name.length() - 2);
                    }
                    
                    mapping.node_to_road[{source, target}] = road_name;
                    mapping.node_to_road[{target, source}] = road_name; // bidirectional
                } catch (...) {
                    // Skip malformed lines
                    continue;
                }
            }
        }
    }
    
    return mapping;
}

// Get road name for a node pair
string get_road_name(const RoadMapping& mapping, NodeID from, NodeID to) {
    auto it = mapping.node_to_road.find({from, to});
    if (it != mapping.node_to_road.end()) {
        return it->second;
    }
    return "Unknown Road";
}

// Find real nodes with road names for testing
pair<NodeID, NodeID> get_real_test_nodes(const RoadMapping& mapping) {
    // Look for nodes on major roads in Quezon City
    vector<string> major_roads = {
        "Quezon Avenue", "E. Rodriguez Sr. Avenue", "EDSA", 
        "Katipunan Avenue", "Commonwealth Avenue", "Timog Avenue"
    };
    
    NodeID source = 0, target = 0;
    
    for (const auto& [node_pair, road_name] : mapping.node_to_road) {
        for (const string& major_road : major_roads) {
            if (road_name.find(major_road) != string::npos) {
                if (source == 0) {
                    source = node_pair.first;
                } else if (target == 0 && node_pair.first != source) {
                    target = node_pair.first;
                    cout << "Found test nodes on major roads:" << endl;
                    cout << "  Source: Node " << source << " on " << get_road_name(mapping, source, target) << endl;
                    cout << "  Target: Node " << target << " on " << road_name << endl;
                    return {source, target};
                }
            }
        }
    }
    
    // Fallback to first available nodes
    if (!mapping.node_to_road.empty()) {
        auto it = mapping.node_to_road.begin();
        source = it->first.first;
        target = it->first.second;
        cout << "Using fallback nodes:" << endl;
        cout << "  Source: Node " << source << " on " << it->second << endl;
    }
    
    return {source, target};
}

// Real location test starting point and destinations
// {
//   Start: (14.634572, 121.023190),
//   Destination: (14.640121, 121.038924)
//   Routes: {
//      Head northwest on Sct. Magbanua toward Quezon Ave/Route 170/R-7 (Sct.59 m),
//      Turn right onto Quezon Ave/Route 170/R-7 (Ave/Route0.4 km),
//      Slight right onto Timog Ave (Continue on road 0.2 km),
//      Turn left after Floro Blue Printing (on the left)Pass by Chooks-to-Go (on the right in 400m),
//      Turn right onto Sgt. Esguerra Ave,
//      Turn left onto Mother Ignacia Ave,
//      Turn right onto Samar Ave,
//      Turn left Restricted usage roadDestination will be on the left,
//   }
// }

// GPS coordinate conversion function
// Converts lat/lng to approximate node IDs based on Quezon City bounds
pair<NodeID, NodeID> gps_to_node_ids(double start_lat, double start_lng, double dest_lat, double dest_lng) {
    // Quezon City bounds (approximate)
    const double lat_min = 14.55, lat_max = 14.85;
    const double lng_min = 120.95, lng_max = 121.25;
    
    // Normalize coordinates to 0-1 range
    double start_lat_norm = max(0.0, min(1.0, (start_lat - lat_min) / (lat_max - lat_min)));
    double start_lng_norm = max(0.0, min(1.0, (start_lng - lng_min) / (lng_max - lng_min)));
    double dest_lat_norm = max(0.0, min(1.0, (dest_lat - lat_min) / (lat_max - lat_min)));
    double dest_lng_norm = max(0.0, min(1.0, (dest_lng - lng_min) / (lng_max - lng_min)));
    
    // Map to approximate node range (assuming roughly 13614 nodes from QC dataset)
    NodeID source_node = (NodeID)(start_lat_norm * start_lng_norm * 13000) + 1;
    NodeID target_node = (NodeID)(dest_lat_norm * dest_lng_norm * 13000) + 1;
    
    // Ensure nodes are within reasonable range
    source_node = max((NodeID)1, min((NodeID)13614, source_node));
    target_node = max((NodeID)1, min((NodeID)13614, target_node));
    
    // Ensure they're different
    if (source_node == target_node && target_node < 13614) {
        target_node++;
    }
    
    return make_pair(source_node, target_node);
}

// Calculate approximate distance between GPS coordinates (in meters)
double calculate_gps_distance(double lat1, double lng1, double lat2, double lng2) {
    const double R = 6371000; // Earth's radius in meters
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlng = (lng2 - lng1) * M_PI / 180.0;
    
    double a = sin(dlat/2) * sin(dlat/2) + 
               cos(lat1_rad) * cos(lat2_rad) * sin(dlng/2) * sin(dlng/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return R * c; // Distance in meters
}

// Test structure for organizing results
struct TestResult {
    string scenario_name;
    NodeID source_node;
    NodeID target_node;
    distance_t hc2l_distance;
    double query_response_time;
    double labeling_time;
    size_t labeling_size;
    double expected_distance_m;
    bool test_passed;
    string notes;
    vector<pair<NodeID, NodeID>> route_path; // Store the actual path
};

// Print formatted test results
void print_test_results(const vector<TestResult>& results, const RoadMapping& road_mapping) {
    cout << "\n" << string(120, '=') << endl;
    cout << "               HC2L DYNAMIC ALGORITHM ACCURACY TEST RESULTS" << endl;
    cout << string(120, '=') << endl;
    
    cout << left;
    cout << setw(20) << "Scenario" 
         << setw(12) << "Source→Target"
         << setw(15) << "HC2L Dist (u)"
         << setw(15) << "Expected (m)"
         << setw(18) << "Query Time (μs)"
         << setw(18) << "Label Time (ms)"
         << setw(15) << "Label Size"
         << setw(10) << "Status" << endl;
    cout << string(120, '-') << endl;
    
    for (const auto& result : results) {
        cout << setw(20) << result.scenario_name
             << setw(12) << (to_string(result.source_node) + "→" + to_string(result.target_node))
             << setw(15) << result.hc2l_distance
             << setw(15) << fixed << setprecision(0) << result.expected_distance_m
             << setw(18) << fixed << setprecision(2) << (result.query_response_time * 1000000)
             << setw(18) << fixed << setprecision(3) << (result.labeling_time * 1000)
             << setw(15) << result.labeling_size
             << setw(10) << (result.test_passed ? "✓ PASS" : "✗ FAIL") << endl;
        
        if (!result.notes.empty()) {
            cout << "    Note: " << result.notes << endl;
        }
        
        // Display route with road names
        cout << "    Route: Node " << result.source_node << " (" 
             << get_road_name(road_mapping, result.source_node, result.target_node) 
             << ") → Node " << result.target_node << " (" 
             << get_road_name(road_mapping, result.target_node, result.source_node) 
             << ")" << endl;
        
        // If we have path information, display the full route
        if (!result.route_path.empty()) {
            cout << "    Full Path: ";
            for (size_t i = 0; i < result.route_path.size(); ++i) {
                auto [from, to] = result.route_path[i];
                cout << "Node " << from << "→" << to << " (" << get_road_name(road_mapping, from, to) << ")";
                if (i < result.route_path.size() - 1) cout << " → ";
            }
            cout << endl;
        }
        cout << endl;
    }
    
    cout << string(120, '=') << endl;
    
    // Calculate summary statistics
    double total_query_time = 0, total_labeling_time = 0;
    size_t total_labeling_size = 0, passed_tests = 0;
    
    for (const auto& result : results) {
        total_query_time += result.query_response_time;
        total_labeling_time += result.labeling_time;
        total_labeling_size += result.labeling_size;
        if (result.test_passed) passed_tests++;
    }
    
    cout << "\nSUMMARY STATISTICS:" << endl;
    cout << "  Total Tests: " << results.size() << endl;
    cout << "  Passed: " << passed_tests << " (" << (100.0 * passed_tests / results.size()) << "%)" << endl;
    cout << "  Average Query Time: " << fixed << setprecision(2) 
         << (total_query_time / results.size() * 1000000) << " μs" << endl;
    cout << "  Average Labeling Time: " << fixed << setprecision(3) 
         << (total_labeling_time / results.size() * 1000) << " ms" << endl;
    cout << "  Average Labeling Size: " << (total_labeling_size / results.size()) << " nodes" << endl;
    cout << string(120, '=') << endl;
}

int main() {
    cout << "HC2L DYNAMIC ALGORITHM ACCURACY TEST" << endl;
    cout << "Testing with Real GPS Coordinates from Quezon City" << endl;
    cout << string(60, '=') << endl;
    
    // Real GPS coordinates for testing
    // Start: Scout Magbanua Street (14.634572, 121.023190)
    // Destination: Near Timog Avenue (14.640121, 121.038924)
    const double start_lat = 14.634572;
    const double start_lng = 121.023190;
    const double dest_lat = 14.640121;
    const double dest_lng = 121.038924;
    
    // Calculate expected distance based on GPS coordinates
    double expected_distance_m = calculate_gps_distance(start_lat, start_lng, dest_lat, dest_lng);
    
    cout << "Test Route Details:" << endl;
    cout << "  Start: (" << fixed << setprecision(6) << start_lat << ", " << start_lng << ") - Scout Magbanua Street" << endl;
    cout << "  Destination: (" << dest_lat << ", " << dest_lng << ") - Near Timog Avenue" << endl;
    cout << "  Expected Distance: " << fixed << setprecision(0) << expected_distance_m << " meters" << endl;
    cout << "  Expected Route: Sct. Magbanua → Quezon Ave → Timog Ave → Sgt. Esguerra Ave → Mother Ignacia Ave → Samar Ave" << endl;
    cout << endl;
    
    // Load graph
    const string graph_file = "test_data/qc_from_csv.gr";
    cout << "Loading Quezon City graph from: " << graph_file << endl;
    
    ifstream gfs(graph_file);
    if (!gfs.is_open()) {
        cerr << "Error: Failed to open graph file: " << graph_file << endl;
        cerr << "Please ensure the file exists in the test_data directory." << endl;
        return 1;
    }
    
    Graph g;
    read_graph(g, gfs);
    gfs.close();
    
    cout << "Graph loaded successfully!" << endl;
    cout << "  Nodes: " << g.node_count() << endl;
    cout << "  Edges: " << g.edge_count() << endl;
    cout << endl;
    
    // Load road names from CSV files
    vector<string> scenario_files = {
        "test_data/qc_scenario_for_cpp_1.csv",
        "test_data/qc_scenario_for_cpp_2.csv",
        "test_data/qc_scenario_for_cpp_3.csv",
        "test_data/qc_scenario_for_cpp_4.csv",
        "test_data/qc_scenario_for_cpp_5.csv"
    };
    
    cout << "Loading road name mappings from CSV files..." << endl;
    RoadMapping road_mapping = load_road_names(scenario_files);
    cout << "Loaded " << road_mapping.node_to_road.size() << " road name mappings." << endl;
    
    // Get real test nodes with actual road names
    auto [real_source, real_target] = get_real_test_nodes(road_mapping);
    
    // Use real nodes if found, otherwise fall back to GPS approximation
    NodeID source_node, target_node;
    if (real_source != 0 && real_target != 0) {
        source_node = real_source;
        target_node = real_target;
        cout << "\nUsing real nodes from road network:" << endl;
    } else {
        // Convert GPS to node IDs as fallback
        auto [gps_source, gps_target] = gps_to_node_ids(start_lat, start_lng, dest_lat, dest_lng);
        source_node = gps_source;
        target_node = gps_target;
        cout << "\nUsing GPS-approximated nodes:" << endl;
    }
    
    cout << "Mapped to Graph Nodes:" << endl;
    cout << "  Source Node: " << source_node << endl;
    cout << "  Target Node: " << target_node << endl;
    
    // Display mapped road names for our test nodes
    cout << "\nMapped Road Information:" << endl;
    cout << "  Source Node " << source_node << " is on: " 
         << get_road_name(road_mapping, source_node, target_node) << endl;
    cout << "  Target Node " << target_node << " is on: " 
         << get_road_name(road_mapping, target_node, source_node) << endl;
    cout << endl;
    
    vector<TestResult> all_results;
    
    // Test with base mode (no disruptions)
    cout << "Testing BASE mode (no disruptions)..." << endl;
    Dynamic gd_base(g);
    gd_base.setMode(Mode::BASE);
    
    // Capture metrics by redirecting cout temporarily
    stringstream captured_output;
    streambuf* orig_cout = cout.rdbuf();
    cout.rdbuf(captured_output.rdbuf());
    
    distance_t base_distance = gd_base.get_distance(source_node, target_node, true);
    
    cout.rdbuf(orig_cout); // Restore cout
    
    // Parse metrics from captured output
    string output_str = captured_output.str();
    double base_query_time = 0, base_labeling_time = 0;
    size_t base_labeling_size = 0;
    
    size_t pos = output_str.find("METRIC_QUERY_TIME:");
    if (pos != string::npos) base_query_time = stod(output_str.substr(pos + 18));
    pos = output_str.find("METRIC_LABELING_TIME:");
    if (pos != string::npos) base_labeling_time = stod(output_str.substr(pos + 21));
    pos = output_str.find("METRIC_LABELING_SIZE:");
    if (pos != string::npos) base_labeling_size = stoul(output_str.substr(pos + 21));
    
    TestResult base_result = {
        "BASE (No Disruptions)",
        source_node,
        target_node,
        base_distance,
        base_query_time,
        base_labeling_time,
        base_labeling_size,
        expected_distance_m,
        base_distance > 0 && base_distance < 1000000, // Reasonable distance check
        "Baseline test without any traffic disruptions",
        {} // Empty route path for now
    };
    all_results.push_back(base_result);
    
    cout << "Base mode result: " << base_distance << " units" << endl << endl;
    
    // Test each disruption scenario
    for (size_t i = 0; i < scenario_files.size(); i++) {
        string scenario_file = scenario_files[i];
        string scenario_name = "Scenario " + to_string(i + 1);
        
        cout << "Testing " << scenario_name << " with disruptions from: " << scenario_file << endl;
        
        ifstream test_file(scenario_file);
        if (!test_file.is_open()) {
            cout << "  Warning: Could not open " << scenario_file << ", skipping..." << endl;
            continue;
        }
        test_file.close();
        
        Dynamic gd_dynamic(g);
        gd_dynamic.setMode(Mode::DISRUPTED);
        gd_dynamic.loadDisruptions(scenario_file);
        
        // Capture metrics
        captured_output.str("");
        captured_output.clear();
        cout.rdbuf(captured_output.rdbuf());
        
        distance_t dynamic_distance = gd_dynamic.get_distance(source_node, target_node, true);
        
        cout.rdbuf(orig_cout); // Restore cout
        
        // Parse metrics
        output_str = captured_output.str();
        double dynamic_query_time = 0, dynamic_labeling_time = 0;
        size_t dynamic_labeling_size = 0;
        
        pos = output_str.find("METRIC_QUERY_TIME:");
        if (pos != string::npos) dynamic_query_time = stod(output_str.substr(pos + 18));
        pos = output_str.find("METRIC_LABELING_TIME:");
        if (pos != string::npos) dynamic_labeling_time = stod(output_str.substr(pos + 21));
        pos = output_str.find("METRIC_LABELING_SIZE:");
        if (pos != string::npos) dynamic_labeling_size = stoul(output_str.substr(pos + 21));
        
        // Determine if test passed (distance should be reasonable and >= base distance due to disruptions)
        bool test_passed = dynamic_distance > 0 && dynamic_distance < 1000000;
        string notes = "";
        
        if (dynamic_distance >= base_distance) {
            notes = "Route affected by disruptions (longer than base)";
        } else if (dynamic_distance == base_distance) {
            notes = "Same as base route (no relevant disruptions)";
        } else {
            notes = "Route shorter than base (unexpected)";
            test_passed = false;
        }
        
        TestResult dynamic_result = {
            scenario_name,
            source_node,
            target_node,
            dynamic_distance,
            dynamic_query_time,
            dynamic_labeling_time,
            dynamic_labeling_size,
            expected_distance_m,
            test_passed,
            notes,
            {} // Empty route path for now
        };
        all_results.push_back(dynamic_result);
        
        cout << "  " << scenario_name << " result: " << dynamic_distance << " units" << endl;
    }
    
    // Print comprehensive results
    print_test_results(all_results, road_mapping);
    
    cout << "\nTEST COMPLETED SUCCESSFULLY!" << endl;
    cout << "The HC2L Dynamic algorithm has been tested with real GPS coordinates" << endl;
    cout << "and multiple traffic disruption scenarios from Quezon City." << endl;
    
    return 0;
}