#include "road_network.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace road_network;

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--in" || std::string(argv[3]) != "--out") {
        std::cerr << "Usage: hc2l_cli_build --in <input.gr> --out <output.index>\n";
        return 1;
    }

    std::string in_file = argv[2];
    std::string out_file = argv[4];

    std::cerr << "[INFO] Reading graph from: " << in_file << "\n";


    // Load graph
    Graph g;
    std::ifstream in(in_file);
    if (!in) {
        std::cerr << "Error opening input file: " << in_file << "\n";
        return 1;
    }

    read_graph(g, in);  // ✅ this is the correct function
    in.close();

    std::cerr << "[INFO] Graph loaded: " << g.get_nodes().size() << " nodes\n";



    // Build cut index
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<CutIndex> ci;
    size_t num_shortcuts = g.create_cut_index(ci, 0.5);  // 0.5 = balance factor
    auto end = std::chrono::high_resolution_clock::now();

    std::cerr << "[INFO] Writing index to: " << out_file << "\n";


    // Write index to file
    std::ofstream out(out_file, std::ios::binary);
    if (!out) {
        std::cerr << "Error opening output file: " << out_file << "\n";
        return 1;
    }

    ContractionIndex contraction_index(ci);  // ✅ wrapper handles compression
    contraction_index.write(out);
    out.close();

    double duration = std::chrono::duration<double>(end - start).count();
    std::cerr << "Labeling size: " << contraction_index.size() / (1024.0 * 1024.0) << " MB\n";
    std::cerr << "Shortcuts used: " << num_shortcuts << "\n";
    std::cerr << "Construction time: " << duration << " s\n";

    return 0;
}
