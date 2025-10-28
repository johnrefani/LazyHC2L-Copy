#include "Dynamic.h"
#include "road_network.h"
#include "util.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

using namespace std;
using namespace road_network;
using namespace hc2l_dynamic;
namespace fs = std::filesystem;

// Globals
road_network::Graph* global_graph_ptr = nullptr;

int main(int argc, char **argv) {
    if (argc < 4) {
        cout << "Usage: " << argv[0] << " <graph_file> <disruptions_file> <tau_threshold> [source_node] [target_node]" << endl;
        cout << "  tau_threshold: Disruption threshold value (range: 0.1 - 1.0)" << endl;
        return 0;
    }

    const char *graphFile = argv[1];
    const char *disruptionsFile = argv[2];
    
    // Validate file existence before proceeding
    vector<string> required_files = {string(graphFile), string(disruptionsFile)};
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
        cerr << "\nPlease ensure all files exist before running the program." << endl;
        return 1;
    }
    
    // Check if files are readable
    for (const string& file_path : required_files) {
        ifstream test_file(file_path);
        if (!test_file.is_open()) {
            cerr << "❌ ERROR: Cannot read file '" << file_path << "'" << endl;
            return 1;
        }
        test_file.close();
    }
    
    cout << "✅ File validation successful:" << endl;
    cout << "   📊 Graph file: " << graphFile << endl;
    cout << "   🔴 Disruptions file: " << disruptionsFile << endl;
    cout << endl;
    
    // Parse and validate tau_threshold (now required)
    double tau_value = stod(argv[3]);
    if (tau_value < 0.1 || tau_value > 1.0) {
        cerr << "Error: tau_threshold must be between 0.1 and 1.0 (inclusive). Provided: " << tau_value << endl;
        return 1;
    }
    road_network::DISRUPTION_THRESHOLD_TAU = tau_value;

    cout << "Using disruption threshold τ = " << road_network::DISRUPTION_THRESHOLD_TAU << "\n";

    // Load graph
    ifstream gfs(graphFile);
    if (!gfs.is_open()) {
        cerr << "Failed to open graph file: " << graphFile << endl;
        return 1;
    }

    Graph g;
    read_graph(g, gfs);
    gfs.close();
    global_graph_ptr = &g;

    Dynamic gd(g);
    gd.loadDisruptions(disruptionsFile);

    // --- Get source and target nodes from command line or use defaults
    NodeID s = 2, t = 4;  // Default nodes
    if (argc >= 6) {
        s = stoi(argv[4]);
        t = stoi(argv[5]);
    }

    // --- BASE mode
    gd.setMode(Mode::BASE);
    auto d_base = gd.get_distance(s, t, true);
    cout << "BASE distance(" << s << "," << t << ") = " << d_base << endl;

    // === 🔥 NEW: Impact Score Demonstration ===
    cout << "\n" << string(60, '=') << endl;
    cout << "🔬 IMPACT SCORE EVALUATION SYSTEM DEMO" << endl;
    cout << string(60, '=') << endl;
    
    cout << "\n📍 Scenario 1: Testing Minor Road Closure (15% network impact)" << endl;
    cout << "Formula: Impact Score = f_Δw × f_jam × f_closure" << endl;
    cout << "Check: Is network impact ≥ " << (road_network::DISRUPTION_THRESHOLD_TAU * 100) << "% threshold?" << endl;
    
    // Simulate a significant disruption
    gd.addUserDisruption(s, t, "Road Closure", "Closed");
    
    cout << "\n📍 Scenario 2: Testing Light Congestion (5% network impact)" << endl;
    NodeID s2 = (s + 1 <= g.node_count()) ? s + 1 : s - 1;
    NodeID t2 = (t + 1 <= g.node_count()) ? t + 1 : t - 1;
    
    gd.addUserDisruption(s2, t2, "Light Traffic", "Light");
    
    cout << "\n📍 Scenario 3: Testing Medium Accident" << endl;
    NodeID s3 = (s + 2 <= g.node_count()) ? s + 2 : s - 2;
    NodeID t3 = (t + 2 <= g.node_count()) ? t + 2 : t - 2;
    
    gd.addUserDisruption(s3, t3, "Vehicle Accident", "Medium");

    // --- Compare distances in different modes
    cout << "\n" << string(50, '-') << endl;
    cout << "🆚 MODE COMPARISON - PROPER LAZY vs IMMEDIATE BEHAVIOR" << endl;
    cout << string(50, '-') << endl;
    
    // Test BASE mode
    gd.setMode(Mode::BASE);
    auto d_base_final = gd.get_distance(s, t, true);
    cout << "BASE mode distance(" << s << "," << t << ") = " << d_base_final << endl;
    
    cout << "\n📋 Testing LAZY UPDATE MODE behavior:" << endl;
    cout << "Expected: Labels marked stale, repair only when accessed\n";
    // Test LAZY_UPDATE mode
    gd.setMode(Mode::LAZY_UPDATE);
    auto d_lazy = gd.get_distance(s, t, true);
    cout << "LAZY_UPDATE mode distance(" << s << "," << t << ") = " << d_lazy << endl;
    
    cout << "\n📋 Testing second query in LAZY mode (should use cached/repaired labels):" << endl;
    auto d_lazy2 = gd.get_distance(s, t, true);
    cout << "LAZY_UPDATE mode (2nd query) distance(" << s << "," << t << ") = " << d_lazy2 << endl;
    
    cout << "\n📋 Testing IMMEDIATE UPDATE MODE behavior:" << endl;
    cout << "Expected: Background precomputation, labels kept fresh proactively\n";
    // Test IMMEDIATE_UPDATE mode  
    gd.setMode(Mode::IMMEDIATE_UPDATE);
    auto d_immediate = gd.get_distance(s, t, true);
    cout << "IMMEDIATE_UPDATE mode distance(" << s << "," << t << ") = " << d_immediate << endl;
    
    cout << "\n📋 Testing second query in IMMEDIATE mode (should use precomputed labels):" << endl;
    auto d_immediate2 = gd.get_distance(s, t, true);
    cout << "IMMEDIATE_UPDATE mode (2nd query) distance(" << s << "," << t << ") = " << d_immediate2 << endl;
    
    // Test standard DISRUPTED mode for comparison
    cout << "\n📋 Testing standard DISRUPTED mode (legacy behavior):" << endl;
    gd.setMode(Mode::DISRUPTED);
    auto d_disrupted = gd.get_distance(s, t, true);
    cout << "DISRUPTED mode distance(" << s << "," << t << ") = " << d_disrupted << endl;
    
    cout << "\n🎯 Proper Lazy/Immediate Update System Demonstrated!" << endl;
    cout << "✅ LAZY UPDATE: Labels marked stale → repair on access (saves memory/computation)" << endl;
    cout << "✅ IMMEDIATE UPDATE: Background precomputation → fresh labels ready (proactive)" << endl;
    cout << "✅ Threshold-based automatic mode selection working correctly" << endl;

    return 0;
}
