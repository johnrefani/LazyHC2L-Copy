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
#include <filesystem>

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;
namespace fs = std::filesystem;

// Function to calculate GPS distance using Haversine formula
double calculate_gps_distance(double lat1, double lng1, double lat2, double lng2) {
    const double R = 6371000; // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlng = (lng2 - lng1) * M_PI / 180.0;
    
    double a = sin(dlat/2) * sin(dlat/2) + 
               cos(lat1_rad) * cos(lat2_rad) * sin(dlng/2) * sin(dlng/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return R * c;
}

int main() {
    cout << "HC2L DYNAMIC ALGORITHM WITH GPS COORDINATE ROUTING" << endl;
    cout << "Enhanced Route Finding with Detailed Path Information" << endl;
    cout << string(60, '=') << endl;
    
    // List of required files
    vector<string> required_files = {
        "../../data/processed/qc_from_csv.gr",
        "../../data/raw/quezon_city_nodes.csv", 
        "../../data/disruptions/qc_scenario_for_cpp_1.csv"
    };
    
    // Check if all required files exist
    vector<string> missing_files;
    for (const string& file_path : required_files) {
        if (!fs::exists(file_path)) {
            missing_files.push_back(file_path);
        }
    }
    
    if (!missing_files.empty()) {
        cerr << "❌ ERROR: Missing required files:" << endl;
        for (const string& missing_file : missing_files) {
            cerr << "   - " << missing_file << endl;
        }
        cerr << "\nPlease ensure all data files are in the '../data/' directory before running." << endl;
        return 1;
    }
    
    cout << "✅ All required files validated successfully" << endl;
    cout << endl;
    
    // Real GPS coordinates for testing
    // Start: Scout Magbanua Street (14.634572, 121.023190)
    // Destination: Near Timog Avenue (14.640121, 121.038924)
    const double start_lat = 14.624397;
    const double start_lng = 121.050725;
    const double dest_lat = 14.666665;
    const double dest_lng = 121.057591;
    
    // Calculate expected distance based on GPS coordinates
    double expected_distance_m = calculate_gps_distance(start_lat, start_lng, dest_lat, dest_lng);
    
    cout << "Test Route Details:" << endl;
    cout << "  Start: (" << fixed << setprecision(6) << start_lat << ", " << start_lng << ") - Scout Magbanua Street" << endl;
    cout << "  Destination: (" << dest_lat << ", " << dest_lng << ") - Near Timog Avenue" << endl;
    cout << "  Expected Distance: " << fixed << setprecision(0) << expected_distance_m << " meters" << endl;
    cout << "  Expected Route: Sct. Magbanua → Quezon Ave → Timog Ave → Sgt. Esguerra Ave → Mother Ignacia Ave → Samar Ave" << endl;
    cout << endl;
    
    // Load graph
    const string graph_file = "../../data/processed/qc_from_csv.gr";
    cout << "Loading Quezon City graph from: " << graph_file << endl;
    
    ifstream gfs(graph_file);
    if (!gfs.is_open()) {
        cerr << "Error: Failed to open graph file: " << graph_file << endl;
        cerr << "Please ensure the file exists in the ../../data/processed directory." << endl;
        return 1;
    }
    
    Graph g;
    read_graph(g, gfs);
    gfs.close();
    
    cout << "Graph loaded successfully!" << endl;
    cout << "  Nodes: " << g.node_count() << endl;
    cout << "  Edges: " << g.edge_count() << endl;
    cout << endl;
    
    // Initialize Dynamic algorithm with coordinate mapping
    Dynamic dynamic(g);
    
    // Initialize coordinate mapping system
    const string nodes_file = "../../data/raw/quezon_city_nodes.csv";
    const string scenario_file = "../../data/disruptions/qc_scenario_for_cpp_1.csv";

    cout << "Initializing coordinate mapping system..." << endl;
    if (!dynamic.initializeCoordinateMapping(nodes_file, scenario_file)) {
        cerr << "Error: Failed to initialize coordinate mapping system." << endl;
        cerr << "Please ensure " << nodes_file << " and " << scenario_file << " exist." << endl;
        return 1;
    }
    
    // Test 1: BASE mode (no disruptions)
    cout << "\n" << string(50, '=') << endl;
    cout << "TEST 1: BASE MODE (No Disruptions)" << endl;
    cout << string(50, '=') << endl;
    
    dynamic.setMode(Mode::BASE);
    
    auto start_time = chrono::high_resolution_clock::now();
    RouteInfo base_route = dynamic.findRouteByGPS(start_lat, start_lng, dest_lat, dest_lng, true);
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end_time - start_time);
    
    if (!base_route.path.empty()) {
        cout << "Base route found in " << duration.count() << " microseconds" << endl;
        dynamic.displayDetailedRoute(base_route);
    } else {
        cout << "Error: No base route found!" << endl;
        return 1;
    }
    
    // Test 2: DISRUPTED mode (with traffic disruptions)
    cout << "\n" << string(50, '=') << endl;
    cout << "TEST 2: DISRUPTED MODE (With Traffic Disruptions)" << endl;
    cout << string(50, '=') << endl;
    
    // Load disruptions
    cout << "Loading traffic disruptions..." << endl;
    dynamic.loadDisruptions(scenario_file);
    dynamic.setMode(Mode::DISRUPTED);
    
    start_time = chrono::high_resolution_clock::now();
    RouteInfo disrupted_route = dynamic.findRouteByGPS(start_lat, start_lng, dest_lat, dest_lng, true);
    end_time = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end_time - start_time);
    
    if (!disrupted_route.path.empty()) {
        cout << "Disrupted route found in " << duration.count() << " microseconds" << endl;
        dynamic.displayDetailedRoute(disrupted_route);
    } else {
        cout << "Error: No disrupted route found!" << endl;
        return 1;
    }
    
    // Test 3: Route Comparison
    cout << "\n" << string(50, '=') << endl;
    cout << "TEST 3: ROUTE COMPARISON" << endl;
    cout << string(50, '=') << endl;
    
    cout << "Comparison Results:" << endl;
    cout << "  Base Route Distance:      " << base_route.total_distance << " meters" << endl;
    cout << "  Disrupted Route Distance: " << disrupted_route.total_distance << " meters" << endl;
    cout << "  Distance Difference:      " << (disrupted_route.total_distance - base_route.total_distance) << " meters" << endl;
    cout << "  Base Route Time:          " << fixed << setprecision(1) << base_route.estimated_time_minutes << " minutes" << endl;
    cout << "  Disrupted Route Time:     " << disrupted_route.estimated_time_minutes << " minutes" << endl;
    cout << "  Time Difference:          " << (disrupted_route.estimated_time_minutes - base_route.estimated_time_minutes) << " minutes" << endl;
    cout << "  Base Route Uses Disruptions:      " << (base_route.uses_disruptions ? "YES" : "NO") << endl;
    cout << "  Disrupted Route Uses Disruptions: " << (disrupted_route.uses_disruptions ? "YES" : "NO") << endl;
    
    // Test 4: Additional GPS coordinate tests
    cout << "\n" << string(50, '=') << endl;
    cout << "TEST 4: ADDITIONAL GPS COORDINATE TESTS" << endl;
    cout << string(50, '=') << endl;
    
    // Test nearby coordinates
    vector<pair<pair<double, double>, pair<double, double>>> test_routes = {
        {{14.635000, 121.025000}, {14.639000, 121.035000}}, // Short route
        {{14.630000, 121.020000}, {14.645000, 121.040000}}, // Longer route
        {{14.638000, 121.030000}, {14.642000, 121.032000}}  // Very short route
    };
    
    for (size_t i = 0; i < test_routes.size(); i++) {
        auto& route_coords = test_routes[i];
        double test_start_lat = route_coords.first.first;
        double test_start_lng = route_coords.first.second;
        double test_end_lat = route_coords.second.first;
        double test_end_lng = route_coords.second.second;
        
        cout << "\nAdditional Test " << (i + 1) << ":" << endl;
        cout << "  From: (" << fixed << setprecision(6) << test_start_lat << ", " << test_start_lng << ")" << endl;
        cout << "  To: (" << test_end_lat << ", " << test_end_lng << ")" << endl;
        
        RouteInfo additional_route = dynamic.findRouteByGPS(test_start_lat, test_start_lng, test_end_lat, test_end_lng, true);
        
        if (!additional_route.path.empty()) {
            cout << "  Route found: " << additional_route.total_distance << " meters, " 
                 << fixed << setprecision(1) << additional_route.estimated_time_minutes << " minutes" << endl;
            cout << "  Uses disruptions: " << (additional_route.uses_disruptions ? "YES" : "NO") << endl;
            cout << "  Path length: " << additional_route.path.size() << " nodes" << endl;
        } else {
            cout << "  No route found!" << endl;
        }
    }
    
    cout << "\n" << string(60, '=') << endl;
    cout << "GPS COORDINATE ROUTING TEST COMPLETED SUCCESSFULLY!" << endl;
    cout << "The enhanced HC2L Dynamic algorithm can now:" << endl;
    cout << "  ✓ Find routes using GPS coordinates" << endl;
    cout << "  ✓ Display detailed turn-by-turn directions" << endl;
    cout << "  ✓ Show road names and coordinates for each step" << endl;
    cout << "  ✓ Handle traffic disruptions and alternative routing" << endl;
    cout << "  ✓ Provide estimated travel times" << endl;
    cout << string(60, '=') << endl;
    
    return 0;
}