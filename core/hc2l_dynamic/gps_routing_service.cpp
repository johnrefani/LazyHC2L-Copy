#include <iostream>
#include <chrono>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "gps_routing_service.h"

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;

// Example usage function
void demonstrateUsage() {
    cout << "\n" << string(65, '=') << endl;
    cout << "🎯 GPS ROUTING SERVICE DEMONSTRATION" << endl;
    cout << string(65, '=') << endl;
    
    // Initialize the service
    GPSRoutingService service;
    cout << "\n📊 Initializing GPS Routing Service..." << endl;
    
    if (!service.initialize()) {
        cout << "❌ Failed to initialize routing service!" << endl;
        return;
    }
    
    cout << "✅ Successfully initialized HC2L routing service" << endl;
    
    // Example coordinates
    double start_lat = 14.647631;
    double start_lng = 121.064644;
    double dest_lat = 14.644476;
    double dest_lng = 121.064569;
    
    cout << "\n🌍 Testing BASE MODE routing:" << endl;
    cout << "📍 From: (" << fixed << setprecision(6) << start_lat << ", " << start_lng << ")" << endl;
    cout << "🎯 To: (" << dest_lat << ", " << dest_lng << ")" << endl;
    
    // Test base mode
    auto base_result = service.findRoute(start_lat, start_lng, dest_lat, dest_lng, false);
    
    if (base_result.success) {
        cout << "BASE MODE Results:" << endl;
        cout << "Query time: " << base_result.query_time_microseconds << " microseconds" << endl;
        cout << " Total distance: " << base_result.total_distance_meters << " meters" << endl;
        cout << " Path length: " << base_result.path_length << " intersections" << endl;
        cout << " GPS → Nodes: " << base_result.gps_to_node_info << endl;
        cout << " Route trace: " << base_result.complete_route_trace << endl;
    } else {
        cout << "❌ BASE MODE failed: " << base_result.error_message << endl;
    }
    
    cout << "\n🔴 Testing DISRUPTED MODE routing:" << endl;
    
    // Test disrupted mode
    auto disrupted_result = service.findRoute(start_lat, start_lng, dest_lat, dest_lng, true);
    
    if (disrupted_result.success) {
        cout << "✅ DISRUPTED MODE Results:" << endl;
        cout << "   ⏱️  Query time: " << disrupted_result.query_time_microseconds << " microseconds" << endl;
        cout << "   📏 Total distance: " << disrupted_result.total_distance_meters << " meters" << endl;
        cout << "   🛣️  Path length: " << disrupted_result.path_length << " intersections" << endl;
        cout << "   📍 GPS → Nodes: " << disrupted_result.gps_to_node_info << endl;
        cout << "   📋 Route trace: " << disrupted_result.complete_route_trace << endl;
        
        if (disrupted_result.had_disruptions) {
            cout << "\n   📊 COMPARISON WITH BASE MODE:" << endl;
            cout << "   🔄 Distance difference: " << fixed << setprecision(1) 
                 << disrupted_result.distance_difference_meters << " meters (" 
                 << disrupted_result.distance_change_percentage << "% change)" << endl;
            cout << "   💬 Analysis: " << disrupted_result.route_comparison << endl;
        }
    } else {
        cout << "❌ DISRUPTED MODE failed: " << disrupted_result.error_message << endl;
    }
    
    cout << "\n" << string(65, '=') << endl;
    cout << "✅ DEMONSTRATION COMPLETE" << endl;
    cout << "🎯 Use GPSRoutingService::findRoute() to get routing data programmatically" << endl;
    cout << string(65, '=') << endl;
}

// Main function for testing
int main() {
    demonstrateUsage();
    return 0;
}