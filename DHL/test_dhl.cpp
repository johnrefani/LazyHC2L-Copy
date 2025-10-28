#include "src/road_network.h"
#include "src/util.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <map>
#include <set>
#include <cmath>

using namespace std;
using namespace road_network;

// Function to convert CSV data to DIMACS format for testing
void convertCSVToDimacs(const string& csvPath, const string& dimacsPath) {
    cout << "Converting CSV data to DIMACS format..." << endl;
    
    ifstream csvFile(csvPath);
    if (!csvFile.is_open()) {
        cerr << "Error: Cannot open CSV file: " << csvPath << endl;
        return;
    }
    
    ofstream dimacsFile(dimacsPath);
    if (!dimacsFile.is_open()) {
        cerr << "Error: Cannot create DIMACS file: " << dimacsPath << endl;
        return;
    }
    
    string line;
    vector<pair<pair<uint32_t, uint32_t>, uint32_t>> rawEdges; // (source, target, weight)
    set<uint32_t> nodeSet;
    
    // Skip header line
    getline(csvFile, line);
    
    int edgeCount = 0;
    while (getline(csvFile, line)) {
        if (line.empty()) continue;
        
        stringstream ss(line);
        string token;
        vector<string> tokens;
        
        while (getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 8) {
            try {
                uint32_t source = stoul(tokens[0]);
                uint32_t target = stoul(tokens[1]);
                
                // Use segment length as weight (convert from meters to integer, scaled down)
                double segmentLength = stod(tokens[7]); // segmentLength column
                uint32_t weight = max(1u, (uint32_t)round(segmentLength / 10.0)); // Scale down for reasonable weights
                
                rawEdges.push_back({{source, target}, weight});
                nodeSet.insert(source);
                nodeSet.insert(target);
                edgeCount++;
                
                // Limit to first 500 edges for more substantial testing
                if (edgeCount >= 500) break;
            } catch (const exception& e) {
                // Skip invalid lines
                continue;
            }
        }
    }
    
    csvFile.close();
    
    // Create node mapping (original ID -> sequential ID starting from 1)
    map<uint32_t, uint32_t> nodeMapping;
    uint32_t nodeCounter = 1;
    for (const auto& nodeId : nodeSet) {
        nodeMapping[nodeId] = nodeCounter++;
    }
    
    // Convert edges using the mapping
    vector<pair<pair<uint32_t, uint32_t>, uint32_t>> edges;
    for (const auto& rawEdge : rawEdges) {
        uint32_t mappedSource = nodeMapping[rawEdge.first.first];
        uint32_t mappedTarget = nodeMapping[rawEdge.first.second];
        edges.push_back({{mappedSource, mappedTarget}, rawEdge.second});
    }
    
    // Write DIMACS format
    dimacsFile << "c DHL Test Graph converted from CSV data" << endl;
    dimacsFile << "c" << endl;
    dimacsFile << "p sp " << nodeSet.size() << " " << edges.size() << endl;
    dimacsFile << "c graph contains " << nodeSet.size() << " nodes and " << edges.size() << " arcs" << endl;
    dimacsFile << "c" << endl;
    
    for (const auto& edge : edges) {
        dimacsFile << "a " << edge.first.first << " " << edge.first.second << " " << edge.second << endl;
    }
    
    dimacsFile.close();
    cout << "Converted " << edges.size() << " edges and " << nodeSet.size() << " nodes to DIMACS format." << endl;
    cout << "Original node ID range: " << *nodeSet.begin() << " to " << *nodeSet.rbegin() << endl;
    cout << "Mapped to sequential IDs: 1 to " << nodeSet.size() << endl;
}

// Function to create sample queries from the graph nodes
void createSampleQueries(const Graph& g, const string& queryPath) {
    cout << "Creating sample queries..." << endl;
    
    ofstream queryFile(queryPath);
    if (!queryFile.is_open()) {
        cerr << "Error: Cannot create query file: " << queryPath << endl;
        return;
    }
    
    const auto& nodes = g.get_nodes();
    if (nodes.size() < 2) {
        cerr << "Error: Not enough nodes for queries" << endl;
        return;
    }
    
    // Create 10 random queries
    srand(42); // Fixed seed for reproducible results
    for (int i = 0; i < min(10, (int)nodes.size() - 1); i++) {
        NodeID source = nodes[rand() % nodes.size()];
        NodeID target = nodes[rand() % nodes.size()];
        
        // Ensure source != target
        while (source == target && nodes.size() > 1) {
            target = nodes[rand() % nodes.size()];
        }
        
        queryFile << source << " " << target << endl;
    }
    
    queryFile.close();
    cout << "Created sample queries." << endl;
}

// Function to create sample updates
void createSampleUpdates(const Graph& g, const string& updatePath) {
    cout << "Creating sample updates..." << endl;
    
    ofstream updateFile(updatePath);
    if (!updateFile.is_open()) {
        cerr << "Error: Cannot create update file: " << updatePath << endl;
        return;
    }
    
    vector<Edge> edges;
    g.get_edges(edges);
    
    if (edges.empty()) {
        cerr << "Error: No edges available for updates" << endl;
        return;
    }
    
    // Create 5 sample updates
    srand(42); // Fixed seed for reproducible results
    for (int i = 0; i < min(5, (int)edges.size()); i++) {
        const Edge& edge = edges[rand() % edges.size()];
        updateFile << edge.a << " " << edge.b << " " << edge.d << endl;
    }
    
    updateFile.close();
    cout << "Created sample updates." << endl;
}

// Test function to demonstrate DHL functionality
void testDHLFunctionality() {
    cout << "\n=== Testing DHL (Dual-Hierarchy Labelling) Functionality ===" << endl;
    
    try {
        // Step 1: Convert CSV to DIMACS format
        string csvPath = "../data/processed/qc_scenario_for_cpp_1.csv";
        string graphPath = "qc_test_graph.txt";
        string queryPath = "qc_test_queries.txt";
        string updatePath = "qc_test_updates.txt";
        
        convertCSVToDimacs(csvPath, graphPath);
        
        // Step 2: Load the graph
        cout << "\nLoading graph from: " << graphPath << endl;
        ifstream graphFile(graphPath);
        if (!graphFile.is_open()) {
            cerr << "Error: Cannot open graph file" << endl;
            return;
        }
        
        Graph g;
        read_graph(g, graphFile);
        graphFile.close();
        
        cout << "Graph loaded successfully!" << endl;
        cout << "Nodes: " << g.node_count() << ", Edges: " << g.edge_count() << endl;
        
        // Step 3: Create sample queries and updates
        createSampleQueries(g, queryPath);
        createSampleUpdates(g, updatePath);
        
        // Step 4: Perform degree-1 node contraction
        cout << "\nPerforming degree-1 node contraction..." << endl;
        vector<Neighbor> closest;
        g.contract(closest);
        cout << "Contraction completed. Contracted nodes: " << closest.size() << endl;
        
        // Step 5: Build the cut index
        cout << "\nBuilding cut index..." << endl;
        util::start_timer();
        vector<CutIndex> ci;
        size_t shortcuts = g.create_cut_index(ci, 0.2); // 0.2 balance parameter
        double indexTime = util::stop_timer();
        
        cout << "Cut index built in " << indexTime << " seconds" << endl;
        cout << "Shortcuts added: " << shortcuts << endl;
        cout << "Cut index size: " << ci.size() << endl;
        
        // Step 6: Reset graph and create contraction hierarchy
        cout << "\nCreating contraction hierarchy..." << endl;
        g.reset();
        ContractionHierarchy ch;
        g.create_contraction_hierarchy(ch, ci, closest);
        
        // Step 7: Build the final contraction index
        cout << "Building contraction index..." << endl;
        ContractionIndex conIndex(ci, closest);
        
        cout << "Index construction completed!" << endl;
        cout << "Index size: " << conIndex.size() / (1024.0 * 1024.0) << " MB" << endl;
        cout << "Average cut size: " << conIndex.avg_cut_size() << endl;
        cout << "Max cut size: " << conIndex.max_cut_size() << endl;
        cout << "Height: " << conIndex.height() << endl;
        
        // Step 8: Test distance queries
        cout << "\n=== Testing Distance Queries ===" << endl;
        
        ifstream queryFile(queryPath);
        if (!queryFile.is_open()) {
            cerr << "Error: Cannot open query file" << endl;
            return;
        }
        
        NodeID source, target;
        int queryCount = 0;
        util::start_timer();
        
        while (queryFile >> source >> target && queryCount < 10) {
            distance_t dist = conIndex.get_distance(source, target);
            cout << "Distance from " << source << " to " << target << ": ";
            
            if (dist == infinity) {
                cout << "INFINITY (unreachable)" << endl;
            } else {
                cout << dist << endl;
            }
            queryCount++;
        }
        
        double queryTime = util::stop_timer();
        queryFile.close();
        
        cout << "Processed " << queryCount << " queries in " << queryTime << " seconds" << endl;
        cout << "Average query time: " << (queryTime / queryCount) << " seconds" << endl;
        
        // Step 9: Test updates (basic demonstration)
        cout << "\n=== Testing Updates ===" << endl;
        
        ifstream updateFile(updatePath);
        if (!updateFile.is_open()) {
            cerr << "Error: Cannot open update file" << endl;
            return;
        }
        
        NodeID a, b;
        distance_t weight;
        int updateCount = 0;
        
        vector<pair<pair<distance_t, distance_t>, pair<NodeID, NodeID>>> updates;
        
        while (updateFile >> a >> b >> weight && updateCount < 5) {
            // Simulate weight increase (1.5x original weight)
            distance_t newWeight = weight * 1.5;
            updates.push_back({{weight, newWeight}, {a, b}});
            
            cout << "Update " << updateCount + 1 << ": Edge (" << a << ", " << b 
                 << ") weight " << weight << " -> " << newWeight << endl;
            updateCount++;
        }
        updateFile.close();
        
        // Apply incremental updates
        if (!updates.empty()) {
            cout << "Applying incremental updates..." << endl;
            util::start_timer();
            g.DhlInc(ch, conIndex, updates);
            double updateTime = util::stop_timer();
            
            cout << "Applied " << updates.size() << " incremental updates in " 
                 << updateTime << " seconds" << endl;
        }
        
        // Step 10: Verify some queries after updates
        cout << "\n=== Verifying Queries After Updates ===" << endl;
        
        ifstream verifyFile(queryPath);
        if (verifyFile.is_open()) {
            int verifyCount = 0;
            while (verifyFile >> source >> target && verifyCount < 3) {
                distance_t dist = conIndex.get_distance(source, target);
                cout << "Post-update distance from " << source << " to " << target << ": ";
                
                if (dist == infinity) {
                    cout << "INFINITY" << endl;
                } else {
                    cout << dist << endl;
                }
                verifyCount++;
            }
            verifyFile.close();
        }
        
        cout << "\n=== DHL Test Completed Successfully! ===" << endl;
        
    } catch (const exception& e) {
        cerr << "Error during DHL testing: " << e.what() << endl;
    }
}

int main() {
    cout << "DHL (Dual-Hierarchy Labelling) Test Program" << endl;
    cout << "===========================================" << endl;
    
    cout << "This program tests the DHL implementation with the Quezon City dataset." << endl;
    cout << "The DHL technique provides fast shortest-path queries with support for dynamic updates." << endl;
    
    testDHLFunctionality();
    
    return 0;
}