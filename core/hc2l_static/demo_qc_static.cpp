#include "../include/road_network.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

using namespace road_network;

struct CSVEdge {
    NodeID source, target;
    std::string road_name;
    double speed_kph, freeFlow_kph, jamFactor;
    bool isClosed;
    double segmentLength;
    int jamTendency, hour_of_day, duration_min;
    std::string location_tag;
    
    CSVEdge() = default;
};

// Parse CSV data and extract node relationships
std::vector<CSVEdge> parseCSV(const std::string& csv_path) {
    std::vector<CSVEdge> edges;
    std::ifstream file(csv_path);
    
    if (!file.good()) {
        std::cerr << "Error: Cannot open CSV file: " << csv_path << std::endl;
        return edges;
    }
    
    std::string line;
    std::getline(file, line); // Skip header
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::vector<std::string> fields;
        std::string field;
        
        // Parse CSV fields
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        
        if (fields.size() >= 12) {
            CSVEdge edge;
            try {
                edge.source = static_cast<NodeID>(std::stoul(fields[0]));
                edge.target = static_cast<NodeID>(std::stoul(fields[1]));
                edge.road_name = fields[2];
                edge.speed_kph = std::stod(fields[3]);
                edge.freeFlow_kph = std::stod(fields[4]);
                edge.jamFactor = std::stod(fields[5]);
                edge.isClosed = (fields[6] == "True");
                edge.segmentLength = std::stod(fields[7]);
                edge.jamTendency = std::stoi(fields[8]);
                edge.hour_of_day = std::stoi(fields[9]);
                edge.location_tag = fields[10];
                edge.duration_min = std::stoi(fields[11]);
                
                edges.push_back(edge);
            } catch (const std::exception& e) {
                // Skip invalid lines
                continue;
            }
        }
    }
    
    return edges;
}

int main() {
    std::cout << "=== HC2L Static + Quezon City CSV Demo ===" << std::endl;
    
    // Load graph
    std::cout << "Loading Quezon City road network graph..." << std::endl;
    Graph graph;
    std::ifstream graph_file("test_data/qc_from_csv.gr");
    if (!graph_file.good()) {
        std::cerr << "Error: Cannot open graph file" << std::endl;
        return 1;
    }
    
    read_graph(graph, graph_file);
    graph_file.close();
    
    std::cout << "Graph loaded: " << graph.node_count() << " nodes, " 
              << graph.edge_count() << " edges" << std::endl;
    
    // Build HC2L index
    std::cout << "Building HC2L cut index..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<CutIndex> ci;
    size_t shortcuts = graph.create_cut_index(ci, 0.5);
    ContractionIndex index(ci);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto build_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Index built in " << build_time.count() << " ms" << std::endl;
    std::cout << "Index size: " << (index.size() / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "Shortcuts: " << shortcuts << std::endl;
    
    // Load and analyze CSV data
    std::cout << "\\nAnalyzing CSV scenario data..." << std::endl;
    std::vector<CSVEdge> csv_edges = parseCSV("test_data/qc_scenario_for_cpp_1.csv");
    std::cout << "CSV data loaded: " << csv_edges.size() << " edge records" << std::endl;
    
    // Extract unique nodes from CSV
    std::set<NodeID> csv_nodes;
    for (const auto& edge : csv_edges) {
        csv_nodes.insert(edge.source);
        csv_nodes.insert(edge.target);
    }
    std::cout << "CSV references " << csv_nodes.size() << " unique nodes" << std::endl;
    
    // Node ID mapping analysis
    std::cout << "\\nNode ID Analysis:" << std::endl;
    NodeID max_csv_node = *std::max_element(csv_nodes.begin(), csv_nodes.end());
    std::cout << "Max CSV node ID: " << max_csv_node << std::endl;
    std::cout << "Graph node count: " << graph.node_count() << std::endl;
    
    // Find nodes that exist in both datasets (intersection)
    std::vector<NodeID> valid_nodes;
    for (NodeID node : csv_nodes) {
        if (node < graph.node_count()) {
            valid_nodes.push_back(node);
        }
    }
    std::cout << "Nodes in valid range: " << valid_nodes.size() << std::endl;
    
    // Distance query validation using CSV node pairs
    std::cout << "\\nTesting distance queries on CSV node pairs..." << std::endl;
    int queries_tested = 0;
    int reachable_pairs = 0;
    std::vector<distance_t> distances;
    
    auto query_start = std::chrono::high_resolution_clock::now();
    
    // Test a sample of CSV edges
    int max_tests = std::min(100, static_cast<int>(csv_edges.size()));
    for (int i = 0; i < max_tests; i++) {
        const auto& edge = csv_edges[i];
        
        // Only test if both nodes are in valid range
        if (edge.source < graph.node_count() && edge.target < graph.node_count()) {
            distance_t dist = index.get_distance(edge.source, edge.target);
            queries_tested++;
            
            if (dist != infinity) {
                reachable_pairs++;
                distances.push_back(dist);
            }
        }
    }
    
    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);
    
    std::cout << "Queries tested: " << queries_tested << std::endl;
    std::cout << "Reachable pairs: " << reachable_pairs << " (" 
              << (100.0 * reachable_pairs / std::max(1, queries_tested)) << "%)" << std::endl;
    
    if (!distances.empty()) {
        std::sort(distances.begin(), distances.end());
        std::cout << "Distance statistics:" << std::endl;
        std::cout << "  Min: " << distances.front() << std::endl;
        std::cout << "  Max: " << distances.back() << std::endl;
        std::cout << "  Median: " << distances[distances.size()/2] << std::endl;
    }
    
    std::cout << "Total query time: " << query_time.count() << " microseconds" << std::endl;
    if (queries_tested > 0) {
        std::cout << "Average query time: " << (query_time.count() / queries_tested) 
                  << " microseconds" << std::endl;
    }
    
    // Traffic condition analysis
    std::cout << "\\nTraffic Condition Analysis:" << std::endl;
    int closed_roads = 0;
    int jammed_roads = 0;
    std::map<std::string, int> location_types;
    
    for (const auto& edge : csv_edges) {
        if (edge.isClosed) closed_roads++;
        if (edge.jamFactor > 2.0) jammed_roads++;
        location_types[edge.location_tag]++;
    }
    
    std::cout << "Closed roads: " << closed_roads << " (" 
              << (100.0 * closed_roads / csv_edges.size()) << "%)" << std::endl;
    std::cout << "Jammed roads (factor > 2.0): " << jammed_roads << " (" 
              << (100.0 * jammed_roads / csv_edges.size()) << "%)" << std::endl;
    
    std::cout << "\\nLocation types in dataset:" << std::endl;
    for (const auto& [location, count] : location_types) {
        std::cout << "  " << location << ": " << count << std::endl;
    }
    
    std::cout << "\\n=== Demo Complete ===" << std::endl;
    std::cout << "The HC2L static algorithm successfully:" << std::endl;
    std::cout << "- Loaded the Quezon City road network (" << graph.node_count() << " nodes)" << std::endl;
    std::cout << "- Built an efficient distance index (" << (index.size() / (1024.0 * 1024.0)) << " MB)" << std::endl;
    std::cout << "- Answered distance queries in microseconds" << std::endl;
    std::cout << "- Analyzed traffic scenario data (" << csv_edges.size() << " records)" << std::endl;
    
    return 0;
}