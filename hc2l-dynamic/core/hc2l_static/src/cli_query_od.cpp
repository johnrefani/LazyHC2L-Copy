#include "road_network.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>

using namespace road_network;

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--index" || std::string(argv[3]) != "--od") {
        std::cerr << "Usage: hc2l_query_od --index <index_file> --od <od_pairs_file>\n";
        return 1;
    }

    std::string index_file = argv[2];
    std::string od_file = argv[4];

    // Load HC2L index
    std::ifstream in(index_file, std::ios::binary);
    if (!in) {
        std::cerr << "Error opening index file: " << index_file << "\n";
        return 1;
    }
    ContractionIndex index(in);
    std::cerr << "[DEBUG] Index loaded. Max valid node ID: " << index.label_count() - 1 << "\n";

    // Load OD pairs
    std::ifstream od_input(od_file);
    if (!od_input) {
        std::cerr << "Error opening OD or result file.\n";
        return 1;
    }

    std::string line;
    std::getline(od_input, line); // Skip header

    std::vector<std::pair<NodeID, NodeID>> od_pairs;
    while (std::getline(od_input, line)) {
        std::stringstream ss(line);
        std::string s_str, t_str;
        std::getline(ss, s_str, ',');
        std::getline(ss, t_str, ',');

        NodeID s = static_cast<NodeID>(std::stoi(s_str));
        NodeID t = static_cast<NodeID>(std::stoi(t_str));
        od_pairs.emplace_back(s, t);
    }

    std::cerr << "[INFO] Loaded " << od_pairs.size() << " OD pairs\n";

    // Output results
    std::ofstream out("results.csv");
    out << "source,target,distance_meters,time_microseconds,disconnected\n";

    for (auto [s, t] : od_pairs) {
        std::cerr << "[QUERY] " << s << " -> " << t << "\n";
        auto start = std::chrono::high_resolution_clock::now();
        distance_t dist = index.get_distance(s, t);
        auto end = std::chrono::high_resolution_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        bool disconnected = (dist == infinity);
        if (disconnected) {
            out << s << "," << t << ",-1," << micros << ",true\n";
        } else {
            out << s << "," << t << "," << dist << "," << micros << ",false\n";
        }
    }

    std::cerr << "[INFO] Queried " << od_pairs.size() << " OD pairs\n";
    return 0;
}
