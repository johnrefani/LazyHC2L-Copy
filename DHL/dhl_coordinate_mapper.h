#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "road_network.h"

namespace dhl {

struct DHLCoordinate {
    road_network::NodeID node_id;
    double latitude;
    double longitude;
    
    DHLCoordinate() = default;
    DHLCoordinate(road_network::NodeID id, double lat, double lng) 
        : node_id(id), latitude(lat), longitude(lng) {}
        
    // Calculate distance to another coordinate using Haversine formula
    double distance_to(const DHLCoordinate& other) const;
};

struct DHLRoadSegment {
    road_network::NodeID source_id;
    road_network::NodeID target_id;
    double source_lat, source_lng;
    double target_lat, target_lng;
    std::string road_name;
    double segment_length;
    double speed_kph;
    double jam_factor;
    bool is_closed;
    
    DHLRoadSegment() = default;
};

class DHLCoordinateMapper {
public:
    // Load node coordinates from CSV file
    bool loadNodeCoordinates(const std::string& nodes_csv_file);
    
    // Load road segments with coordinates from scenario CSV
    bool loadRoadSegments(const std::string& scenario_csv_file);
    
    // Find the nearest node to given GPS coordinates
    road_network::NodeID findNearestNode(double latitude, double longitude, double& distance_m) const;
    
    // Get coordinates for a specific node
    bool getNodeCoordinates(road_network::NodeID node_id, double& latitude, double& longitude) const;
    
    // Get road name for an edge between two nodes
    std::string getRoadName(road_network::NodeID source, road_network::NodeID target) const;
    
    // Get detailed road segment information
    bool getRoadSegment(road_network::NodeID source, road_network::NodeID target, DHLRoadSegment& segment) const;
    
    // Calculate haversine distance between two GPS coordinates in meters
    static double calculateDistance(double lat1, double lng1, double lat2, double lng2);
    
    // Get all node coordinates (for debugging)
    const std::vector<DHLCoordinate>& getAllNodes() const { return node_coordinates; }
    
    // Get statistics
    size_t getNodeCount() const { return node_coordinates.size(); }
    size_t getRoadSegmentCount() const { return road_segments.size(); }

private:
    std::vector<DHLCoordinate> node_coordinates;
    std::vector<DHLRoadSegment> road_segments;
    std::unordered_map<road_network::NodeID, size_t> node_index_map;
    
    // Custom hash function for std::pair<NodeID, NodeID>
    struct PairHasher {
        std::size_t operator()(const std::pair<road_network::NodeID, road_network::NodeID>& p) const {
            auto h1 = std::hash<road_network::NodeID>{}(p.first);
            auto h2 = std::hash<road_network::NodeID>{}(p.second);
            return h1 ^ (h2 << 1);  
        }
    };
    
    std::unordered_map<std::pair<road_network::NodeID, road_network::NodeID>, size_t, PairHasher> segment_map;
};

} // namespace dhl