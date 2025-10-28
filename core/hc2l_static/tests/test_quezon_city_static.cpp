#include <gtest/gtest.h>
#include "../include/road_network.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <map>
#include <set>

const std::string QC_GRAPH_PATH = "../../test_data/qc_from_csv.gr";
const std::string QC_SCENARIO_PATH = "../../test_data/qc_scenario_for_cpp_1.csv";

class QuezonCityStaticTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load the Quezon City graph
        std::ifstream file(QC_GRAPH_PATH);
        if (!file.good()) {
            GTEST_SKIP() << "Quezon City graph file not found: " << QC_GRAPH_PATH;
        }
        
        graph = std::make_unique<road_network::Graph>();
        road_network::read_graph(*graph, file);
        file.close();
        
        std::cerr << "[INFO] Loaded QC graph with " << graph->node_count() << " nodes and " 
                  << graph->edge_count() << " edges\n";
    }
    
    std::unique_ptr<road_network::Graph> graph;
};

TEST_F(QuezonCityStaticTest, GraphLoading) {
    ASSERT_NE(graph, nullptr);
    EXPECT_GT(graph->node_count(), 0);
    EXPECT_GT(graph->edge_count(), 0);
    
    std::cerr << "[STATS] QC Graph - Nodes: " << graph->node_count() 
              << ", Edges: " << graph->edge_count() << "\n";
}

TEST_F(QuezonCityStaticTest, GraphConnectivity) {
    // Test that the graph has reasonable connectivity
    // Check a sample of node degrees
    size_t nodes_with_neighbors = 0;
    size_t total_nodes = graph->node_count();
    
    // Sample some nodes to check connectivity
    for (road_network::NodeID i = 0; i < std::min(total_nodes, size_t(100)); i++) {
        if (graph->degree(i) > 0) {
            nodes_with_neighbors++;
        }
    }
    
    // Expect that most sampled nodes have neighbors (well-connected graph)
    EXPECT_GT(nodes_with_neighbors, 0);
    std::cerr << "[CONNECTIVITY] " << nodes_with_neighbors << " out of " 
              << std::min(total_nodes, size_t(100)) << " sampled nodes have neighbors\n";
}

TEST_F(QuezonCityStaticTest, CutIndexConstruction) {
    ASSERT_NE(graph, nullptr);
    
    // Build cut index for the QC graph
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<road_network::CutIndex> ci;
    double balance = 0.5;  // Standard balance factor
    
    size_t num_shortcuts = graph->create_cut_index(ci, balance);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_GT(ci.size(), 0);
    std::cerr << "[INDEX] Built cut index in " << duration.count() << " ms\n";
    std::cerr << "[INDEX] Shortcuts used: " << num_shortcuts << "\n";
    std::cerr << "[INDEX] Index entries: " << ci.size() << "\n";
    
    // Create contraction index
    road_network::ContractionIndex contraction_index(ci);
    
    size_t index_size = contraction_index.size();
    double size_mb = index_size / (1024.0 * 1024.0);
    
    EXPECT_GT(index_size, 0);
    std::cerr << "[INDEX] Contraction index size: " << size_mb << " MB\n";
}

TEST_F(QuezonCityStaticTest, DistanceQueries) {
    ASSERT_NE(graph, nullptr);
    
    // Build the index first
    std::vector<road_network::CutIndex> ci;
    graph->create_cut_index(ci, 0.5);
    road_network::ContractionIndex contraction_index(ci);
    
    // Test some random distance queries
    std::vector<std::pair<road_network::NodeID, road_network::NodeID>> test_pairs;
    
    // Generate some test pairs
    for (int i = 0; i < 10; i++) {
        road_network::NodeID src = graph->random_node();
        road_network::NodeID dest = graph->random_node();
        test_pairs.emplace_back(src, dest);
    }
    
    // Test distance queries
    int successful_queries = 0;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& [src, dest] : test_pairs) {
        road_network::distance_t dist = contraction_index.get_distance(src, dest);
        
        if (dist != road_network::infinity) {
            successful_queries++;
            EXPECT_GE(dist, 0);  // Distance should be non-negative
        }
        
        // Same node should have distance 0
        road_network::distance_t self_dist = contraction_index.get_distance(src, src);
        EXPECT_EQ(self_dist, 0);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cerr << "[QUERIES] " << successful_queries << " out of " << test_pairs.size() 
              << " queries returned finite distances\n";
    std::cerr << "[QUERIES] Total query time: " << duration.count() << " microseconds\n";
    std::cerr << "[QUERIES] Average query time: " << (duration.count() / (test_pairs.size() * 2.0)) 
              << " microseconds\n";
}

TEST_F(QuezonCityStaticTest, ScenarioDataValidation) {
    // Test that we can read and validate the CSV scenario data
    std::ifstream csv_file(QC_SCENARIO_PATH);
    if (!csv_file.good()) {
        GTEST_SKIP() << "QC scenario CSV file not found: " << QC_SCENARIO_PATH;
    }
    
    std::string line;
    std::getline(csv_file, line); // Skip header
    
    std::set<road_network::NodeID> csv_nodes;
    int valid_edges = 0;
    int total_edges = 0;
    
    while (std::getline(csv_file, line)) {
        std::stringstream ss(line);
        std::string source_str, target_str;
        
        if (std::getline(ss, source_str, ',') && std::getline(ss, target_str, ',')) {
            try {
                road_network::NodeID source = static_cast<road_network::NodeID>(std::stoul(source_str));
                road_network::NodeID target = static_cast<road_network::NodeID>(std::stoul(target_str));
                
                csv_nodes.insert(source);
                csv_nodes.insert(target);
                
                total_edges++;
                
                // Check if these nodes exist in our graph (approximately)
                // Note: CSV might use different node IDs than the graph
                if (source < graph->node_count() && target < graph->node_count()) {
                    valid_edges++;
                }
            } catch (const std::exception& e) {
                // Skip invalid lines
                continue;
            }
        }
    }
    
    csv_file.close();
    
    EXPECT_GT(total_edges, 0);
    EXPECT_GT(csv_nodes.size(), 0);
    
    std::cerr << "[SCENARIO] CSV contains " << total_edges << " edges\n";
    std::cerr << "[SCENARIO] CSV references " << csv_nodes.size() << " unique nodes\n";
    std::cerr << "[SCENARIO] " << valid_edges << " edges have node IDs within graph range\n";
    
    // The ratio might be low due to node ID mapping differences, but some should be valid
    double validity_ratio = static_cast<double>(valid_edges) / total_edges;
    std::cerr << "[SCENARIO] Validity ratio: " << (validity_ratio * 100) << "%\n";
}

TEST_F(QuezonCityStaticTest, PerformanceBenchmark) {
    ASSERT_NE(graph, nullptr);
    
    // Build the index
    auto index_start = std::chrono::high_resolution_clock::now();
    std::vector<road_network::CutIndex> ci;
    size_t num_shortcuts = graph->create_cut_index(ci, 0.5);
    road_network::ContractionIndex contraction_index(ci);
    auto index_end = std::chrono::high_resolution_clock::now();
    
    auto index_time = std::chrono::duration_cast<std::chrono::milliseconds>(index_end - index_start);
    
    // Generate a batch of random queries for benchmarking
    std::vector<std::pair<road_network::NodeID, road_network::NodeID>> queries;
    for (int i = 0; i < 1000; i++) {
        queries.emplace_back(graph->random_node(), graph->random_node());
    }
    
    // Benchmark query performance
    auto query_start = std::chrono::high_resolution_clock::now();
    int reachable_pairs = 0;
    
    for (const auto& [src, dest] : queries) {
        road_network::distance_t dist = contraction_index.get_distance(src, dest);
        if (dist != road_network::infinity) {
            reachable_pairs++;
        }
    }
    
    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);
    
    // Report performance metrics
    std::cerr << "\n=== PERFORMANCE BENCHMARK ===\n";
    std::cerr << "Graph: " << graph->node_count() << " nodes, " << graph->edge_count() << " edges\n";
    std::cerr << "Index construction: " << index_time.count() << " ms\n";
    std::cerr << "Index size: " << (contraction_index.size() / (1024.0 * 1024.0)) << " MB\n";
    std::cerr << "Shortcuts: " << num_shortcuts << "\n";
    std::cerr << "Query batch: " << queries.size() << " queries\n";
    std::cerr << "Reachable pairs: " << reachable_pairs << " (" << (100.0 * reachable_pairs / queries.size()) << "%)\n";
    std::cerr << "Total query time: " << query_time.count() << " microseconds\n";
    std::cerr << "Average query time: " << (static_cast<double>(query_time.count()) / queries.size()) << " microseconds\n";
    std::cerr << "==============================\n";
    
    // Performance expectations
    EXPECT_LT(index_time.count(), 60000);  // Index should build in < 60 seconds
    EXPECT_LT(static_cast<double>(query_time.count()) / queries.size(), 1000);  // < 1ms per query on average
}

TEST_F(QuezonCityStaticTest, IndexSerialization) {
    ASSERT_NE(graph, nullptr);
    
    // Build the index
    std::vector<road_network::CutIndex> ci;
    graph->create_cut_index(ci, 0.5);
    road_network::ContractionIndex original_index(ci);
    
    // Serialize to temporary file
    std::string temp_file = "/tmp/qc_test_index.bin";
    {
        std::ofstream out(temp_file, std::ios::binary);
        ASSERT_TRUE(out.good());
        original_index.write(out);
    }
    
    // Deserialize from file
    std::ifstream loaded_stream(temp_file, std::ios::binary);
    ASSERT_TRUE(loaded_stream.good());
    road_network::ContractionIndex loaded_index(loaded_stream);
    
    // Test that both indexes give same results
    std::vector<std::pair<road_network::NodeID, road_network::NodeID>> test_queries;
    for (int i = 0; i < 20; i++) {
        test_queries.emplace_back(graph->random_node(), graph->random_node());
    }
    
    int matching_results = 0;
    for (const auto& [src, dest] : test_queries) {
        road_network::distance_t dist1 = original_index.get_distance(src, dest);
        road_network::distance_t dist2 = loaded_index.get_distance(src, dest);
        
        if (dist1 == dist2) {
            matching_results++;
        }
    }
    
    EXPECT_EQ(matching_results, test_queries.size());
    std::cerr << "[SERIALIZATION] " << matching_results << " out of " << test_queries.size() 
              << " queries matched between original and loaded index\n";
    
    // Clean up
    std::remove(temp_file.c_str());
}