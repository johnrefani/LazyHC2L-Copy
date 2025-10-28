#include "dhl_coordinate_mapper.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <iostream>
#include <algorithm>

namespace dhl {

// Helper function to parse CSV line with quoted fields
static std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);
    return fields;
}

double DHLCoordinate::distance_to(const DHLCoordinate& other) const {
    return DHLCoordinateMapper::calculateDistance(latitude, longitude, other.latitude, other.longitude);
}

bool DHLCoordinateMapper::loadNodeCoordinates(const std::string& nodes_csv_file) {
    std::ifstream file(nodes_csv_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open nodes file: " << nodes_csv_file << std::endl;
        return false;
    }
    
    node_coordinates.clear();
    node_index_map.clear();
    
    std::string header;
    std::getline(file, header); // Skip header: node_id,latitude,longitude
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::vector<std::string> fields = parseCSVLine(line);
        if (fields.size() < 3) continue;
        
        try {
            road_network::NodeID node_id = std::stoull(fields[0]);
            double latitude = std::stod(fields[1]);
            double longitude = std::stod(fields[2]);
            
            DHLCoordinate coord(node_id, latitude, longitude);
            node_index_map[node_id] = node_coordinates.size();
            node_coordinates.push_back(coord);
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Error parsing node line: " << line << std::endl;
            continue;
        }
    }
    
    file.close();
    std::cerr << "Loaded " << node_coordinates.size() << " node coordinates." << std::endl;
    return !node_coordinates.empty();
}

bool DHLCoordinateMapper::loadRoadSegments(const std::string& scenario_csv_file) {
    std::ifstream file(scenario_csv_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open scenario file: " << scenario_csv_file << std::endl;
        return false;
    }
    
    road_segments.clear();
    segment_map.clear();
    
    std::string header;
    std::getline(file, header); // Skip header
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::vector<std::string> fields = parseCSVLine(line);
        if (fields.size() < 12) continue;
        
        try {
            // Skip lines with missing critical coordinate data
            if (fields[0].empty() || fields[1].empty() || fields[2].empty() || fields[3].empty() ||
                fields[4].empty() || fields[5].empty()) {
                continue; // Skip silently - these are incomplete road segments
            }
            
            DHLRoadSegment segment;
            segment.source_lat = std::stod(fields[0]);
            segment.source_lng = std::stod(fields[1]);
            segment.target_lat = std::stod(fields[2]);
            segment.target_lng = std::stod(fields[3]);
            segment.source_id = std::stoull(fields[4]);
            segment.target_id = std::stoull(fields[5]);
            segment.road_name = fields[6];
            
            // Handle optional fields with defaults
            segment.speed_kph = fields[7].empty() ? 30.0 : std::stod(fields[7]);
            segment.jam_factor = (fields.size() > 9 && !fields[9].empty()) ? std::stod(fields[9]) : 1.0;
            segment.is_closed = (fields[10] == "True" || fields[10] == "true");
            segment.segment_length = (fields.size() > 11 && !fields[11].empty()) ? std::stod(fields[11]) : 0.0;
            
            // Clean up road name
            if (!segment.road_name.empty() && segment.road_name.front() == '"' && segment.road_name.back() == '"') {
                segment.road_name = segment.road_name.substr(1, segment.road_name.length() - 2);
            }
            
            segment_map[{segment.source_id, segment.target_id}] = road_segments.size();
            road_segments.push_back(segment);
            
        } catch (const std::exception& e) {
            // Only show warnings for lines that should have been parseable
            if (!fields[0].empty() && !fields[1].empty() && !fields[2].empty() && !fields[3].empty()) {
                std::cerr << "Warning: Error parsing segment line: " << line << std::endl;
            }
            continue;
        }
    }
    
    file.close();
    std::cerr << "Loaded " << road_segments.size() << " road segments with coordinates." << std::endl;
    return !road_segments.empty();
}

double DHLCoordinateMapper::calculateDistance(double lat1, double lng1, double lat2, double lng2) {
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

road_network::NodeID DHLCoordinateMapper::findNearestNode(double latitude, double longitude, double& distance_m) const {
    if (node_coordinates.empty()) {
        distance_m = std::numeric_limits<double>::max();
        return 0;
    }
    
    road_network::NodeID nearest_node = 0;
    double min_distance = std::numeric_limits<double>::max();
    
    for (const auto& coord : node_coordinates) {
        double dist = calculateDistance(latitude, longitude, coord.latitude, coord.longitude);
        if (dist < min_distance) {
            min_distance = dist;
            nearest_node = coord.node_id;
        }
    }
    
    distance_m = min_distance;
    return nearest_node;
}

bool DHLCoordinateMapper::getNodeCoordinates(road_network::NodeID node_id, double& latitude, double& longitude) const {
    auto it = node_index_map.find(node_id);
    if (it == node_index_map.end()) {
        return false;
    }
    
    const DHLCoordinate& coord = node_coordinates[it->second];
    latitude = coord.latitude;
    longitude = coord.longitude;
    return true;
}

std::string DHLCoordinateMapper::getRoadName(road_network::NodeID source, road_network::NodeID target) const {
    auto key = std::make_pair(source, target);
    auto it = segment_map.find(key);
    if (it != segment_map.end()) {
        return road_segments[it->second].road_name;
    }
    
    // Try reverse direction
    key = std::make_pair(target, source);
    it = segment_map.find(key);
    if (it != segment_map.end()) {
        return road_segments[it->second].road_name;
    }
    
    return "Unknown Road";
}

bool DHLCoordinateMapper::getRoadSegment(road_network::NodeID source, road_network::NodeID target, DHLRoadSegment& segment) const {
    auto key = std::make_pair(source, target);
    auto it = segment_map.find(key);
    if (it != segment_map.end()) {
        segment = road_segments[it->second];
        return true;
    }
    
    // Try reverse direction
    key = std::make_pair(target, source);
    it = segment_map.find(key);
    if (it != segment_map.end()) {
        segment = road_segments[it->second];
        return true;
    }
    
    return false;
}

} // namespace dhl