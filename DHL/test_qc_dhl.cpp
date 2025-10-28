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
    cout << "Converting Quezon City CSV data to DIMACS format..." << endl;
    
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
    cout << "CSV Header: " << line.substr(0, 100) << "..." << endl;
    
    int edgeCount = 0;
    int validLines = 0;
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
                
                // Skip self-loops
                if (source == target) continue;
                
                // Use segment length as weight (convert from meters to integer, scaled down)
                double segmentLength = stod(tokens[7]); // segmentLength column
                uint32_t weight = max(1u, (uint32_t)round(segmentLength / 10.0)); // Scale down for reasonable weights
                
                rawEdges.push_back({{source, target}, weight});
                nodeSet.insert(source);
                nodeSet.insert(target);
                edgeCount++;
                validLines++;
                
                // Limit to first 1000 edges for comprehensive testing
                if (edgeCount >= 1000) break;
            } catch (const exception& e) {
                // Skip invalid lines
                continue;
            }
        }
    }
    
    csvFile.close();
    
    cout << "Processed " << validLines << " valid edges from CSV" << endl;
    
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
    dimacsFile << "c DHL Test Graph converted from Quezon City CSV data" << endl;
    dimacsFile << "c Original dataset: qc_scenario_for_cpp_1.csv" << endl;
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

// Function to create realistic queries from the graph
void createQCQueries(const Graph& g, const string& queryPath) {
    cout << "Creating sample queries for Quezon City network..." << endl;
    
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
    
    // Create 20 diverse queries
    srand(42); // Fixed seed for reproducible results
    for (int i = 0; i < min(20, (int)nodes.size() - 1); i++) {
        NodeID source, target;
        
        // Create diverse query pairs
        if (i < 5) {
            // Short distance queries (nearby nodes)
            source = nodes[i];
            target = nodes[min(i + 10, (int)nodes.size() - 1)];
        } else if (i < 10) {
            // Medium distance queries
            source = nodes[i * nodes.size() / 20];
            target = nodes[(i + 10) * nodes.size() / 20];
        } else {
            // Long distance queries (far apart nodes)
            source = nodes[rand() % (nodes.size() / 2)];
            target = nodes[nodes.size() / 2 + rand() % (nodes.size() / 2)];
        }
        
        // Ensure source != target
        while (source == target && nodes.size() > 1) {
            target = nodes[rand() % nodes.size()];
        }
        
        queryFile << source << " " << target << endl;
    }
    
    queryFile.close();
    cout << "Created sample queries for Quezon City dataset." << endl;
}

// Test function to demonstrate DHL functionality with QC data
void testDHLWithQCData() {
    cout << "\n=== Testing DHL with Quezon City Dataset ===" << endl;
    
    try {
        // Step 1: Convert CSV to DIMACS format
        string csvPath = "../data/processed/qc_scenario_for_cpp_1.csv";
        string graphPath = "quezon_city_graph.txt";
        string queryPath = "quezon_city_queries.txt";
        
        convertCSVToDimacs(csvPath, graphPath);
        
        // Step 2: Load the graph
        cout << "\nLoading Quezon City graph from: " << graphPath << endl;
        ifstream graphFile(graphPath);
        if (!graphFile.is_open()) {
            cerr << "Error: Cannot open graph file" << endl;
            return;
        }
        
        Graph g;
        read_graph(g, graphFile);
        graphFile.close();
        
        cout << "Quezon City graph loaded successfully!" << endl;
        cout << "Network Statistics:" << endl;
        cout << "  Nodes: " << g.node_count() << endl;
        cout << "  Edges: " << g.edge_count() << endl;
        
        // Step 3: Create realistic queries
        createQCQueries(g, queryPath);
        
        // Step 4: Perform degree-1 node contraction
        cout << "\nPerforming graph preprocessing..." << endl;
        vector<Neighbor> closest;
        util::start_timer();
        g.contract(closest);
        double contractionTime = util::stop_timer();
        
        cout << "Contraction completed in " << contractionTime << " seconds" << endl;
        cout << "Contracted nodes: " << closest.size() << endl;
        
        // Step 5: Build the cut index
        cout << "\nBuilding DHL hierarchical index..." << endl;
        util::start_timer();
        vector<CutIndex> ci;
        size_t shortcuts = g.create_cut_index(ci, 0.2); // 0.2 balance parameter
        double indexTime = util::stop_timer();
        
        cout << "Cut index construction completed!" << endl;
        cout << "  Construction time: " << indexTime << " seconds" << endl;
        cout << "  Shortcuts added: " << shortcuts << endl;
        cout << "  Cut index size: " << ci.size() << " labels" << endl;
        
        // Step 6: Reset graph and create contraction hierarchy
        cout << "\nCreating contraction hierarchy..." << endl;
        g.reset();
        ContractionHierarchy ch;
        g.create_contraction_hierarchy(ch, ci, closest);
        
        // Step 7: Build the final contraction index
        cout << "Building final query index..." << endl;
        util::start_timer();
        ContractionIndex conIndex(ci, closest);
        double finalIndexTime = util::stop_timer();
        
        cout << "\nDHL Index Construction Summary:" << endl;
        cout << "================================" << endl;
        cout << "  Total construction time: " << (contractionTime + indexTime + finalIndexTime) << " seconds" << endl;
        cout << "  Index size: " << (conIndex.size() / (1024.0 * 1024.0)) << " MB" << endl;
        cout << "  Average cut size: " << conIndex.avg_cut_size() << endl;
        cout << "  Maximum cut size: " << conIndex.max_cut_size() << endl;
        cout << "  Hierarchy height: " << conIndex.height() << endl;
        cout << "  Label count: " << conIndex.label_count() << endl;
        
        // Step 8: Test distance queries on Quezon City network
        cout << "\n=== Testing Distance Queries on Quezon City Network ===" << endl;
        
        ifstream queryFile(queryPath);
        if (!queryFile.is_open()) {
            cerr << "Error: Cannot open query file" << endl;
            return;
        }
        
        NodeID source, target;
        int queryCount = 0;
        vector<distance_t> queryResults;
        vector<double> queryTimes;
        
        cout << "Processing queries..." << endl;
        while (queryFile >> source >> target && queryCount < 15) {
            util::start_timer();
            distance_t dist = conIndex.get_distance(source, target);
            double singleQueryTime = util::stop_timer();
            
            queryResults.push_back(dist);
            queryTimes.push_back(singleQueryTime);
            
            cout << "Query " << (queryCount + 1) << ": Distance from " << source << " to " << target << " = ";
            
            if (dist == infinity) {
                cout << "INFINITY (unreachable)" << endl;
            } else {
                cout << dist << " units (" << (singleQueryTime * 1000000) << " μs)" << endl;
            }
            queryCount++;
        }
        
        queryFile.close();
        
        // Calculate statistics
        double totalQueryTime = 0;
        for (double t : queryTimes) totalQueryTime += t;
        
        cout << "\nQuery Performance Summary:" << endl;
        cout << "=========================" << endl;
        cout << "  Total queries processed: " << queryCount << endl;
        cout << "  Total query time: " << (totalQueryTime * 1000000) << " μs" << endl;
        cout << "  Average query time: " << (totalQueryTime / queryCount * 1000000) << " μs" << endl;
        cout << "  Queries per second: " << (queryCount / totalQueryTime) << endl;
        
        // Count reachable vs unreachable
        int reachable = 0, unreachable = 0;
        for (distance_t dist : queryResults) {
            if (dist == infinity) unreachable++;
            else reachable++;
        }
        
        cout << "  Reachable pairs: " << reachable << endl;
        cout << "  Unreachable pairs: " << unreachable << endl;
        
        // Step 9: Verify correctness with Dijkstra on a few queries
        cout << "\n=== Correctness Verification ===" << endl;
        g.reset(); // Reset for Dijkstra verification
        
        queryFile.open(queryPath);
        int verificationCount = 0;
        while (queryFile >> source >> target && verificationCount < 3) {
            distance_t indexDist = conIndex.get_distance(source, target);
            distance_t dijkstraDist = g.get_distance(source, target, true);
            
            cout << "Verification " << (verificationCount + 1) << " - Query (" << source << ", " << target << "): ";
            cout << "DHL=" << indexDist << ", Dijkstra=" << dijkstraDist;
            
            if (indexDist == dijkstraDist) {
                cout << " ✓ CORRECT" << endl;
            } else {
                cout << " ✗ MISMATCH!" << endl;
            }
            verificationCount++;
        }
        queryFile.close();
        
        cout << "\n=== Quezon City DHL Test Completed Successfully! ===" << endl;
        cout << "The DHL implementation successfully processed the Quezon City road network dataset." << endl;
        
    } catch (const exception& e) {
        cerr << "Error during DHL testing: " << e.what() << endl;
    }
}

int main() {
    cout << "DHL (Dual-Hierarchy Labelling) Test with Quezon City Dataset" << endl;
    cout << "============================================================" << endl;
    
    cout << "This program tests the DHL implementation using real road network data" << endl;
    cout << "from Quezon City, Philippines. DHL provides fast shortest-path queries" << endl;
    cout << "with support for dynamic network updates." << endl;
    
    testDHLWithQCData();
    
    return 0;
}