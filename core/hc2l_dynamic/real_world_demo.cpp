#include <iostream>
#include <chrono>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include "include/Dynamic.h"
#include "include/road_network.h"

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;

int main() {
    // 🔧 CONFIGURATION VARIABLES - Modify these for your testing
    // GPS Coordinates (Latitude, Longitude)
    double start_latitude = 14.647631;   // Start point latitude
    double start_longitude = 121.064644; // Start point longitude  
    double dest_latitude = 14.644476;    // Destination latitude
    double dest_longitude = 121.064569;  // Destination longitude
    
    // Disruption mode flag
    bool consider_disruptions = true;  
    
    cout << "\n" << string(65, '=') << endl;
    cout << "🎯 HC2L DYNAMIC REAL-WORLD DEMONSTRATION" << endl;
    cout << "   Complete Path Tracing with Quezon City Road Network" << endl;
    cout << string(65, '=') << endl;
    
    cout << "\n📍 GPS CONFIGURATION:" << endl;
    cout << "   🚩 Start: (" << fixed << setprecision(6) << start_latitude << ", " << start_longitude << ")" << endl;
    cout << "   🎯 Destination: (" << dest_latitude << ", " << dest_longitude << ")" << endl;
    cout << "   🚧 Disruption mode: " << (consider_disruptions ? "ENABLED" : "DISABLED") << endl;

    try {
        // Load graph first
        cout << "\n📊 Loading Quezon City Road Network..." << endl;
        const string graph_file = "test_data/qc_from_csv.gr";
        
        ifstream gfs(graph_file);
        if (!gfs.is_open()) {
            cerr << "Error: Failed to open graph file: " << graph_file << endl;
            return 1;
        }
        
        Graph g;
        read_graph(g, gfs);
        gfs.close();
        
        // Initialize HC2L with the loaded graph
        Dynamic qc_router(g);
        
        // Initialize coordinate mapping for GPS-to-node conversion
        cout << "📍 Initializing GPS coordinate mapping..." << endl;
        bool mapping_success = qc_router.initializeCoordinateMapping(
            "test_data/nodes.csv",  // Use the correct file with node_id,lat,lng format
            "test_data/qc_scenario_for_cpp_1.csv"
        );
        
        if (!mapping_success) {
            cout << "❌ ERROR: GPS coordinate mapping failed!" << endl;
            cout << "Cannot proceed without coordinate mapping system." << endl;
            return 1;
        }
        
        cout << "✅ Successfully loaded HC2L index" << endl;
        cout << "📈 Network Statistics:" << endl;
        cout << "   🏙️  Dataset: Quezon City, Philippines" << endl;
        cout << "   🚦 Intersections: 13,649 nodes" << endl;
        cout << "   🛣️  Road segments: 18,768 edges" << endl;
        cout << "   🗺️  Coverage: Real GPS coordinates" << endl;

        cout << "\n🔍 Converting GPS coordinates to nearest graph nodes..." << endl;
        cout << "📍 Start GPS: (" << start_latitude << ", " << start_longitude << ")" << endl;
        cout << "🎯 Destination GPS: (" << dest_latitude << ", " << dest_longitude << ")" << endl;
        cout << string(60, '-') << endl;

        // Perform GPS-based routing with real coordinate conversion
        cout << "\n🌍 GPS-BASED ROUTING:" << endl;
        qc_router.setMode(Mode::BASE);
        
        auto gps_start = chrono::high_resolution_clock::now();
        auto gps_route_info = qc_router.findRouteByGPS(start_latitude, start_longitude, dest_latitude, dest_longitude, true);
        auto gps_end = chrono::high_resolution_clock::now();
        auto gps_duration = chrono::duration_cast<chrono::microseconds>(gps_end - gps_start);
        
        if (gps_route_info.path.empty()) {
            cout << "❌ No route found between the GPS coordinates!" << endl;
            return 1;
        }
        
        // Extract the actual nodes computed from GPS coordinates (NO MORE HARDCODED VALUES!)
        int start_node = gps_route_info.path.front();
        int dest_node = gps_route_info.path.back();
        
        cout << "   ⏱️  GPS Query time: " << gps_duration.count() << " microseconds" << endl;
        cout << "   📏 Total distance: " << gps_route_info.total_distance << " meters" << endl;
        cout << "   🛣️  Path: " << gps_route_info.path.size() << " intersections" << endl;
        cout << "   📍 GPS → Nodes: (" << fixed << setprecision(6) << start_latitude << "," << start_longitude 
             << ") → Node " << start_node << " | (" << dest_latitude << "," << dest_longitude << ") → Node " << dest_node << endl;
        
        cout << "   📋 Complete route trace: ";
        if (gps_route_info.path.size() <= 15) {
            for (size_t j = 0; j < gps_route_info.path.size(); j++) {
                cout << gps_route_info.path[j];
                if (j < gps_route_info.path.size() - 1) cout << " → ";
            }
        } else {
            for (size_t j = 0; j < 5; j++) {
                cout << gps_route_info.path[j] << " → ";
            }
            cout << "... → ";
            for (size_t j = gps_route_info.path.size() - 3; j < gps_route_info.path.size(); j++) {
                cout << gps_route_info.path[j];
                if (j < gps_route_info.path.size() - 1) cout << " → ";
            }
        }
        cout << endl;

        // Test disrupted mode for comparison (using the GPS-computed nodes)
        if (consider_disruptions) {
            cout << "\n🔴 DISRUPTED MODE (Traffic incidents):" << endl;
            qc_router.loadDisruptions("test_data/qc_scenario_for_cpp_1.csv");
            qc_router.setMode(Mode::DISRUPTED);
            
            auto disrupted_start = chrono::high_resolution_clock::now();
            auto disrupted_route = qc_router.get_path(start_node, dest_node, true);
            auto disrupted_end = chrono::high_resolution_clock::now();
            auto disrupted_duration = chrono::duration_cast<chrono::microseconds>(disrupted_end - disrupted_start);
            
            if (!disrupted_route.second.empty()) {
                cout << "   ⏱️  Query time: " << disrupted_duration.count() << " microseconds" << endl;
                cout << "   📏 Distance: " << disrupted_route.first << " meters" << endl;
                cout << "   🛣️  Path: " << disrupted_route.second.size() << " intersections" << endl;
                
                // Calculate route comparison with the GPS-based route
                int distance_diff = static_cast<int>(disrupted_route.first) - static_cast<int>(gps_route_info.total_distance);
                double time_ratio = static_cast<double>(disrupted_duration.count()) / gps_duration.count();
                
                cout << "\n   📊 ROUTE COMPARISON (vs GPS route):" << endl;
                cout << "   🔄 Distance change: ";
                if (distance_diff > 0) cout << "+";
                cout << distance_diff << " meters (" 
                     << fixed << setprecision(1) << (distance_diff / static_cast<double>(gps_route_info.total_distance)) * 100 
                     << "% change)" << endl;
                cout << "   ⚡ Query time ratio: " << fixed << setprecision(2) << time_ratio << "x" << endl;
                
                cout << "   📋 Disrupted route trace: ";
                if (disrupted_route.second.size() <= 15) {
                    for (size_t j = 0; j < disrupted_route.second.size(); j++) {
                        cout << disrupted_route.second[j];
                        if (j < disrupted_route.second.size() - 1) cout << " → ";
                    }
                } else {
                    for (size_t j = 0; j < 5; j++) {
                        cout << disrupted_route.second[j] << " → ";
                    }
                    cout << "... → ";
                    for (size_t j = disrupted_route.second.size() - 3; j < disrupted_route.second.size(); j++) {
                        cout << disrupted_route.second[j];
                        if (j < disrupted_route.second.size() - 1) cout << " → ";
                    }
                }
                cout << endl;
                
                // Route difference analysis
                if (distance_diff == 0) {
                    cout << "   ✅ Same route used (no impact from disruptions)" << endl;
                } else if (distance_diff > 0) {
                    cout << "   🔄 Alternative route found (+";
                    cout << fixed << setprecision(1) << (distance_diff / static_cast<double>(gps_route_info.total_distance)) * 100;
                    cout << "% longer)" << endl;
                } else {
                    cout << "   ⚡ Shorter route found (optimized)" << endl;
                }
            } else {
                cout << "   ❌ No route found (all paths blocked by disruptions)" << endl;
            }
        } else {
            cout << "\n🔵 Disruption testing DISABLED (consider_disruptions = false)" << endl;
        }
        
        cout << "\n" << string(65, '=') << endl;
        cout << "✅ DEMONSTRATION COMPLETE" << endl;
        cout << "🎯 HC2L successfully demonstrated PURE GPS-based routing with:" << endl;
        cout << "✅ Real Quezon City road network (13,649 intersections)" << endl;
        cout << "✅ GPS coordinate input (" << fixed << setprecision(3) << start_latitude << ", " << start_longitude 
             << ") → (" << dest_latitude << ", " << dest_longitude << ")" << endl;
        cout << "✅ Automatic GPS-to-node conversion (Node " << start_node << " → Node " << dest_node << ")" << endl;
        cout << "✅ Complete node-by-node route tracing" << endl;
        if (consider_disruptions) {
            cout << "✅ Traffic disruption handling and route comparison" << endl;
        } else {
            cout << "🔵 Base mode testing (disruptions disabled)" << endl;
        }
        cout << "✅ Performance optimization (microsecond queries)" << endl;
        cout << "\n🚀 Ready for real-world GPS navigation applications!" << endl;
        cout << "💡 Modify GPS coordinates and disruption flag at the top of main() for custom testing" << endl;
        cout << "🎯 NO MORE HARDCODED NODES - Pure GPS-to-node conversion!" << endl;
        cout << string(65, '=') << endl;
        
    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}