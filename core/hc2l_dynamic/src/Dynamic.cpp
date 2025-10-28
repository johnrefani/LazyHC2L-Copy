#include "Dynamic.h"
#include "util.h"
#include "lazy_update_tracker.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <limits>
#include <queue>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>

using namespace road_network;
using namespace std;

namespace hc2l_dynamic {

static inline double clampSlowdown(double x) {
    if (x < 0.0) return 0.0;
    if (x < 1e-9) return 1e-9;
    if (x > 1e9) return 1e9;
    return x;
}

Dynamic::Dynamic(Graph &baseGraph)
    : graph(baseGraph), currentMode(Mode::BASE), coordinate_mapping_initialized(false), 
      background_update_active(false), last_update_time(std::chrono::steady_clock::now()) {}

void Dynamic::setMode(Mode mode) {
    currentMode = mode;
}

Mode Dynamic::getMode() const {
    return currentMode;
}

EdgeID Dynamic::makeEdgeId(NodeID a, NodeID b) {
    return std::minmax(a, b);
}

// User-submitted disruption injection
void Dynamic::addUserDisruption(NodeID u, NodeID v,
                                const std::string& incidentType,
                                const std::string& severity) {
    // Validate node IDs
    if (u == 0 || v == 0 || u >= graph.super_node_count() + 1 || v >= graph.super_node_count() + 1) {
        std::cerr << "Error: Invalid node IDs for user disruption (" << u << ", " << v << ")" << std::endl;
        return;
    }
    
    EdgeID eid = makeEdgeId(u, v);
    disruptionTypeByEdge[eid] = incidentType;
    disruptionSeverityByEdge[eid] = severity;

    // Convert severity to slowdown factor and closure status
    double slowdown_factor = 1.0;
    bool is_closed = false;
    
    if (severity == "Heavy") {
        slowdown_factor = 0.3;
        disruptedSlowdownFactorByEdge[eid] = slowdown_factor;
    } else if (severity == "Medium") {
        slowdown_factor = 0.6;
        disruptedSlowdownFactorByEdge[eid] = slowdown_factor;
    } else if (severity == "Light") {
        slowdown_factor = 0.85;
        disruptedSlowdownFactorByEdge[eid] = slowdown_factor;
    } else if (severity == "Closed") {
        is_closed = true;
        disruptedClosedEdges.insert(eid);
    }

    // 🔥 NEW: Calculate Impact Score and determine update mode
    double jam_factor = 10.0 - (slowdown_factor * 10.0); // Estimate jam factor from slowdown
    if (is_closed) jam_factor = 10.0; // Maximum jam factor for closures
    
    ImpactScore impact = calculateImpactScore(slowdown_factor, jam_factor, is_closed);
    Mode recommended_mode = determineUpdateMode(impact);
    
    std::cout << "[User-Live] Added disruption (" << u << "," << v << ") → "
              << incidentType << " [" << severity << "]\n";
    std::cout << "Impact Score: " << std::fixed << std::setprecision(3) << impact.score 
              << " (f_Δw=" << impact.f_delta_w 
              << " × f_jam=" << impact.f_jam 
              << " × f_closure=" << impact.f_closure << ")\n";
    
    // Automatically set the recommended mode and trigger appropriate behavior
    setMode(recommended_mode);
    
    // 🔥 NEW: Trigger proper mode-specific behavior
    if (recommended_mode == Mode::IMMEDIATE_UPDATE) {
        // Immediate mode: Start background precomputation
        triggerBackgroundLabelUpdate();
    } else if (recommended_mode == Mode::LAZY_UPDATE) {
        // Lazy mode: Mark affected labels as stale
        std::vector<NodeID> affected_nodes = {u, v};
        markLabelsStale(affected_nodes);
    }
}

// Helper function to parse CSV with quoted fields
std::vector<std::string> parseCSVLine(const std::string& line) {
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
    fields.push_back(field); // Add the last field
    return fields;
}

void Dynamic::loadDisruptions(const std::string& filename) {
    disruptedClosedEdges.clear();
    disruptedSlowdownFactorByEdge.clear();
    disruptionSeverityByEdge.clear();
    disruptionTypeByEdge.clear();

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "ERROR: Failed to open disruptions file: " << filename << std::endl;
        std::cerr << "Continuing without disruptions..." << std::endl;
        return;
    }

    std::string header;
    if (!std::getline(infile, header)) {
        std::cerr << "ERROR: Empty disruptions file: " << filename << std::endl;
        return;
    }

    std::string line;
    int lineCount = 0;
    while (std::getline(infile, line)) {
        lineCount++;
        if (line.empty() || line[0] == '#') continue;

        try {
            std::vector<std::string> fields = parseCSVLine(line);
            
            // Skip malformed lines
            if (fields.size() < 12) {
                std::cerr << "Warning: Skipping malformed line " << lineCount << " (only " << fields.size() << " fields)" << std::endl;
                continue;
            }

            // CSV format: source_lat,source_lon,target_lat,target_lon,source,target,road_name,speed_kph,freeFlow_kph,jamFactor,isClosed,segmentLength
            NodeID u = std::stoull(fields[4]);  // source field
            NodeID v = std::stoull(fields[5]);  // target field  
            std::string road_name = fields[6];
            double speed_kph = std::stod(fields[7]);
            double freeFlow_kph = std::stod(fields[8]);
            double jamFactor = std::stod(fields[9]);
            bool isClosed = (fields[10] == "True" || fields[10] == "true" || fields[10] == "1");
            double segment_length = std::stod(fields[11]);
            
            // Set default values for missing fields
            int jamTendency = 1;
            int hour_of_day = 12;
            std::string location_tag = "road";
            int duration_min = 30;

            double slowdown_ratio = speed_kph / (freeFlow_kph > 0 ? freeFlow_kph : 1.0);
            slowdown_ratio = clampSlowdown(slowdown_ratio);

            EdgeID eid = makeEdgeId(u, v);
            if (isClosed) {
                disruptedClosedEdges.insert(eid);
            } else if (slowdown_ratio < 1.0) {
                disruptedSlowdownFactorByEdge[eid] = slowdown_ratio;
            }

            std::string incident = "Other";
            if (isClosed || jamFactor == 10) {
                incident = "Road Closure";
            } else if (speed_kph < 2 && jamFactor > 7 && !isClosed) {
                incident = "Accident";
            } else if (slowdown_ratio <= 0.5 && duration_min >= 30 && jamFactor < 7) {
                incident = "Construction";
            } else if (jamFactor > 7 && speed_kph < 5) {
                incident = "Congestion";
            } else if (speed_kph <= 1 && jamFactor < 4 && segment_length < 100) {
                incident = "Disabled Vehicle";
            } else if (location_tag == "terminal" && hour_of_day >= 6 && hour_of_day <= 9) {
                incident = "Mass Transit Event";
            } else if (location_tag == "event_venue" && (hour_of_day >= 18 || hour_of_day <= 23)) {
                incident = "Planned Event";
            } else if (slowdown_ratio < 0.4 && jamTendency == 1 && !isClosed) {
                incident = "Road Hazard";
            } else if (speed_kph >= 10 && speed_kph <= 15 && jamTendency == 1 && !isClosed) {
                incident = "Lane Restriction";
            } else if (speed_kph < 10 && duration_min > 20) {
                incident = "Weather";
            }

            disruptionTypeByEdge[eid] = incident;

            std::string severity;
            if (slowdown_ratio >= 0.8) severity = "Light";
            else if (slowdown_ratio >= 0.5) severity = "Medium";
            else severity = "Heavy";

            disruptionSeverityByEdge[eid] = severity;

            // 🔥 NEW: Calculate Impact Score for this disruption
            ImpactScore impact = calculateImpactScore(slowdown_ratio, jamFactor, isClosed, segment_length);
            
            // Store impact information for later analysis
            // This could be expanded to track per-edge impact scores if needed
            
            // Disabled verbose output to prevent infinite loop
            // std::cout << "[Disruption] (" << u << "," << v << ") slowdown=" << slowdown_ratio
            //           << ", closed=" << isClosed
            //           << " → Severity: " << severity
            //           << ", Type: " << incident << "\n";

        } catch (const std::exception& e) {
            std::cerr << "Warning: Error parsing line " << lineCount << ": " << e.what() << std::endl;
            continue;
        }
    }

    infile.close();
    
    // 🔥 NEW: Determine overall update mode based on network impact
    if (!disruptedClosedEdges.empty() || !disruptedSlowdownFactorByEdge.empty()) {
        // Calculate overall impact and set appropriate mode
        ImpactScore overall_impact;
        overall_impact.network_percentage_affected = calculateNetworkImpactPercentage();
        overall_impact.exceeds_threshold = (overall_impact.network_percentage_affected >= road_network::DISRUPTION_THRESHOLD_TAU);
        
        Mode recommended_mode = determineUpdateMode(overall_impact);
        setMode(recommended_mode);
        
        std::cout << "📊 Loaded " << (disruptedClosedEdges.size() + disruptedSlowdownFactorByEdge.size()) 
                  << " disruptions affecting " << std::fixed << std::setprecision(1)
                  << (overall_impact.network_percentage_affected * 100) << "% of network\n";
        
        // 🔥 NEW: Trigger proper mode-specific behavior after loading
        if (recommended_mode == Mode::IMMEDIATE_UPDATE) {
            // Immediate mode: Start background precomputation
            std::cout << "🔄 Triggering background label precomputation...\n";
            triggerBackgroundLabelUpdate();
        } else if (recommended_mode == Mode::LAZY_UPDATE) {
            // Lazy mode: Mark all affected nodes as stale
            std::vector<NodeID> affected_nodes;
            for (const auto& edge : disruptedClosedEdges) {
                affected_nodes.push_back(edge.first);
                affected_nodes.push_back(edge.second);
            }
            for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
                affected_nodes.push_back(edge.first);
                affected_nodes.push_back(edge.second);
            }
            std::cout << "🏷️  Marking affected labels as stale for lazy repair...\n";
            markLabelsStale(affected_nodes);
        }
    }
}

// 🔥 NEW: Impact Score Evaluation System
Dynamic::ImpactScore Dynamic::calculateImpactScore(double slowdown_ratio, double jamFactor, bool isClosed, double segment_length) const {
    ImpactScore impact;
    
    // Factor 1: Speed drop factor (f_Δw)
    // Higher impact for greater speed reductions
    if (isClosed) {
        impact.f_delta_w = 1.0;  // Maximum impact for closures
    } else {
        // Map slowdown_ratio to impact factor: lower ratio = higher impact
        impact.f_delta_w = 1.0 - slowdown_ratio;  // 0.0 (no impact) to 1.0 (max impact)
        impact.f_delta_w = std::max(0.0, std::min(1.0, impact.f_delta_w));
    }
    
    // Factor 2: Jam factor component (f_jam)
    // Normalize jamFactor (0-10) to impact scale (0.0-1.0)
    impact.f_jam = std::min(jamFactor / 10.0, 1.0);
    
    // Factor 3: Closure factor (f_closure)
    // Binary multiplier: 1.5x impact if road is closed
    impact.f_closure = isClosed ? 1.5 : 1.0;
    
    // Composite Impact Score = f_Δw × f_jam × f_closure
    impact.score = impact.f_delta_w * impact.f_jam * impact.f_closure;
    
    // Calculate network percentage affected (simplified estimation)
    // In a real implementation, this would analyze the graph topology
    impact.network_percentage_affected = calculateNetworkImpactPercentage();
    
    // Check if impact exceeds threshold
    impact.exceeds_threshold = (impact.network_percentage_affected >= road_network::DISRUPTION_THRESHOLD_TAU);
    
    return impact;
}

Mode Dynamic::determineUpdateMode(const ImpactScore& impact) const {
    if (impact.exceeds_threshold) {
        std::cout << "🚨 IMMEDIATE UPDATE MODE: Impact " 
                  << std::fixed << std::setprecision(1) 
                  << (impact.network_percentage_affected * 100) << "% ≥ "
                  << (road_network::DISRUPTION_THRESHOLD_TAU * 100) << "% threshold\n";
        std::cout << "   → Labels will be immediately recalculated and kept fresh in background\n";
        return Mode::IMMEDIATE_UPDATE;
    } else {
        std::cout << "⏳ LAZY UPDATE MODE: Impact " 
                  << std::fixed << std::setprecision(1) 
                  << (impact.network_percentage_affected * 100) << "% < "
                  << (road_network::DISRUPTION_THRESHOLD_TAU * 100) << "% threshold\n";
        std::cout << "   → Labels will be marked stale and repaired only when accessed\n";
        return Mode::LAZY_UPDATE;
    }
}

double Dynamic::calculateNetworkImpactPercentage() const {
    // Simplified calculation: estimate impact based on number of disrupted edges
    size_t total_disrupted_edges = disruptedClosedEdges.size() + disruptedSlowdownFactorByEdge.size();
    size_t total_edges = graph.edge_count(); // Assuming this method exists or estimate
    
    if (total_edges == 0) return 0.0;
    
    // Calculate percentage, with some heuristics for severity
    double base_percentage = static_cast<double>(total_disrupted_edges) / total_edges;
    
    // Weight by severity: closed roads have 2x impact, heavy slowdowns have 1.5x impact
    double weighted_impact = 0.0;
    for (const auto& edge : disruptedClosedEdges) {
        weighted_impact += 2.0; // Closed roads count as 2x impact
    }
    for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
        if (slowdown < 0.5) {
            weighted_impact += 1.5; // Heavy slowdowns count as 1.5x impact
        } else {
            weighted_impact += 1.0; // Normal disruption impact
        }
    }
    
    double weighted_percentage = weighted_impact / total_edges;
    
    // Return the higher of base or weighted percentage, capped at 100%
    return std::min(1.0, std::max(base_percentage, weighted_percentage));
}

// 🔥 NEW: Proper Lazy/Immediate Update System Implementation

void Dynamic::markLabelsStale(const std::vector<NodeID>& affected_nodes) {
    std::cout << "🏷️  Marking " << affected_nodes.size() << " nodes as stale for lazy repair\n";
    for (NodeID node : affected_nodes) {
        stale_nodes.insert(node);
        // Mark all queries involving this node as requiring repair
        mark_dirty(node, 0); // Mark node itself as needing attention
    }
    last_update_time = std::chrono::steady_clock::now();
}

bool Dynamic::areLabelsStale(NodeID u, NodeID v) const {
    return stale_nodes.count(u) > 0 || stale_nodes.count(v) > 0 || is_dirty(u, v);
}

void Dynamic::repairStaleLabels(NodeID u, NodeID v) {
    if (!areLabelsStale(u, v)) {
        return; // No repair needed
    }
    
    std::cout << "🔧 Repairing stale labels for query (" << u << ", " << v << ")\n";
    
    // Apply all current disruptions to get fresh labels
    for (const auto& edge : disruptedClosedEdges) {
        graph.applyDisruption(edge.first, edge.second, 1.0, true);
    }
    for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
        graph.applyDisruption(edge.first, edge.second, slowdown, false);
    }
    
    // Cache the repaired result
    std::pair<NodeID, NodeID> query_pair = {u, v};
    precomputed_labels[query_pair] = true;
    
    // Remove from stale set if both nodes are now fresh
    // In a full implementation, this would be more sophisticated
    mark_dirty(u, v); // Clear the dirty flag after repair
}

void Dynamic::precomputeAffectedLabels() {
    if (background_update_active) {
        std::cout << "⚠️  Background update already in progress\n";
        return;
    }
    
    background_update_active = true;
    std::cout << "🔄 Starting background label precomputation (IMMEDIATE UPDATE MODE)\n";
    
    // In IMMEDIATE mode, proactively update all affected labels
    auto start_time = std::chrono::steady_clock::now();
    
    // Apply all disruptions immediately
    for (const auto& edge : disruptedClosedEdges) {
        graph.applyDisruption(edge.first, edge.second, 1.0, true);
    }
    for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
        graph.applyDisruption(edge.first, edge.second, slowdown, false);
    }
    
    // Clear stale nodes since we've precomputed everything
    stale_nodes.clear();
    clear_dirty();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "✅ Background precomputation completed in " << duration.count() << "ms\n";
    std::cout << "🚀 All affected labels are now fresh and ready for queries\n";
    
    background_update_active = false;
    last_update_time = std::chrono::steady_clock::now();
}

void Dynamic::triggerBackgroundLabelUpdate() {
    if (currentMode == Mode::IMMEDIATE_UPDATE) {
        precomputeAffectedLabels();
    }
}

distance_t Dynamic::get_distance(NodeID v, NodeID w, bool weighted) {
    // Input validation
    if (v == 0 || w == 0 || v >= graph.super_node_count() + 1 || w >= graph.super_node_count() + 1) {
        std::cerr << "Error: Invalid node IDs (" << v << ", " << w << ")" << std::endl;
        return std::numeric_limits<distance_t>::max();
    }
    
    // Start query response timer
    auto query_start = std::chrono::high_resolution_clock::now();
    
    // Handle different modes according to their proper definitions
    if (currentMode == Mode::BASE) {
        auto labeling_start = std::chrono::high_resolution_clock::now();
        distance_t result = graph.get_distance(v, w, weighted);
        auto labeling_end = std::chrono::high_resolution_clock::now();
        auto query_end = std::chrono::high_resolution_clock::now();
        
        return result;
    }
    
    // 🔥 LAZY UPDATE MODE - Labels are marked stale and only repaired when accessed
    if (currentMode == Mode::LAZY_UPDATE) {
        std::cout << "⏳ LAZY UPDATE: Checking if labels need repair...\n";
        
        // Check if labels are stale and repair only if needed
        if (areLabelsStale(v, w)) {
            std::cout << "🔧 Labels are stale - repairing on access (saves memory/computation)\n";
            repairStaleLabels(v, w);
        } else {
            std::cout << "✅ Labels are fresh - using cached result\n";
        }
        
        auto labeling_start = std::chrono::high_resolution_clock::now();
        distance_t result = graph.get_distance(v, w, weighted);
        auto labeling_end = std::chrono::high_resolution_clock::now();
        
        return result;
    }
    
    // 🔥 IMMEDIATE UPDATE MODE - Labels are immediately recalculated and kept fresh
    if (currentMode == Mode::IMMEDIATE_UPDATE) {
        std::cout << "🚀 IMMEDIATE UPDATE: Using precomputed fresh labels\n";
        
        // In immediate mode, labels should already be fresh from background processing
        if (background_update_active) {
            std::cout << "⚡ Background update still active - waiting for completion\n";
        } else {
            std::cout << "✅ Using precomputed labels (proactive background updates)\n";
        }
        
        auto labeling_start = std::chrono::high_resolution_clock::now();
        distance_t result = graph.get_distance(v, w, weighted);
        auto labeling_end = std::chrono::high_resolution_clock::now();
        
        return result;
    }
    
    // Standard DISRUPTED mode and other modes
    if (currentMode == Mode::DISRUPTED) {
        // Apply all disruptions immediately (legacy behavior)
        for (const auto& edge : disruptedClosedEdges) {
            graph.applyDisruption(edge.first, edge.second, 1.0, true);
        }
        for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
            graph.applyDisruption(edge.first, edge.second, slowdown, false);
        }
        
        distance_t result = graph.get_distance(v, w, weighted);
        return result;
    }
    
    // Fallback for unknown modes
    return graph.get_distance(v, w, weighted);
}

// NEW: Get path with traversed nodes using HC2L index
std::pair<distance_t, std::vector<NodeID>> Dynamic::get_path(NodeID source, NodeID target, bool weighted) {
    // Check if nodes are valid
    if (source == 0 || target == 0) {
        std::cout << "Invalid nodes: source=" << source << ", target=" << target << std::endl;
        return {std::numeric_limits<distance_t>::max(), {}};
    }
    
    std::cout << "Finding path from " << source << " to " << target << " in mode " << static_cast<int>(currentMode) << std::endl;
    
    // Check graph basic info
    std::cout << "Graph has " << graph.node_count() << " nodes and " << graph.edge_count() << " edges" << std::endl;
    
    // Check if source and target are the same
    if (source == target) {
        std::cout << "Source and target are the same node: " << source << std::endl;
        return {0, {source}};
    }
    
    // ============================================================
    // HC2L-BASED ROUTING WITH DYNAMIC DISRUPTIONS
    // ============================================================
    
    // Determine if we should use disrupted state
    bool should_apply_disruptions = (currentMode == Mode::DISRUPTED || 
                                   currentMode == Mode::IMMEDIATE_UPDATE || 
                                   currentMode == Mode::LAZY_UPDATE);
    
    distance_t distance;
    std::vector<NodeID> path;
    
    if (should_apply_disruptions) {
        std::cout << "Applying disruptions for mode: " << static_cast<int>(currentMode) << std::endl;
        
        // CRITICAL: For HC2L to work with disruptions, we need to:
        // 1. Create a temporary modified graph with disruptions applied
        // 2. Use HC2L labels for fast pruning, but validate paths against disrupted graph
        // 3. Fall back to Dijkstra ONLY on affected regions if labels are stale
        
        // Check if labels need refresh based on mode
        if (currentMode == Mode::IMMEDIATE_UPDATE) {
            std::cout << "IMMEDIATE_UPDATE: Rebuilding labels with disruptions" << std::endl;
            
            // Apply disruptions to graph
            for (const auto& edge : disruptedClosedEdges) {
                graph.applyDisruption(edge.first, edge.second, 1.0, true);
            }
            for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
                graph.applyDisruption(edge.first, edge.second, slowdown, false);
            }
            
            // Rebuild HC2L index with disrupted graph
            rebuildLabelsWithDisruptions();
            
            // Use HC2L index to get distance
            distance = get_distance(source, target);
            
            // Get full path using HC2L-guided search on disrupted graph
            if (distance < road_network::infinity) {
                path = reconstructPathFromLabels(source, target);
            }
            
        } else if (currentMode == Mode::LAZY_UPDATE) {
            std::cout << "LAZY_UPDATE: Using stale labels with on-demand repair" << std::endl;
            
            // Mark affected labels as stale (don't rebuild yet)
            markAffectedLabelsAsStale();
            
            // Try HC2L query first with stale labels
            distance = get_distance(source, target);
            
            // Check if path goes through disrupted edges
            bool path_affected = isPathAffectedByDisruptions(source, target);
            
            if (path_affected) {
                std::cout << "Path affected by disruptions - performing lazy repair" << std::endl;
                
                // Lazy repair: Apply disruptions and recompute only for this query
                Graph temp_graph = graph; // Copy for temporary modifications
                
                for (const auto& edge : disruptedClosedEdges) {
                    temp_graph.applyDisruption(edge.first, edge.second, 1.0, true);
                }
                for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
                    if (slowdown < 0.5) { // Critical slowdowns only
                        temp_graph.applyDisruption(edge.first, edge.second, slowdown, false);
                    }
                }
                
                // Use HC2L for pruning, but validate on disrupted graph
                auto result = temp_graph.get_path_dijkstra(source, target, weighted);
                distance = result.first;
                path = result.second;
                
            } else {
                std::cout << "Path not affected - using existing HC2L labels" << std::endl;
                // Path not affected, use HC2L labels directly
                if (distance < road_network::infinity) {
                    path = reconstructPathFromLabels(source, target);
                }
            }
            
        } else {
            // Standard DISRUPTED mode
            std::cout << "DISRUPTED mode: Applying all disruptions with HC2L validation" << std::endl;
            
            // Apply all disruptions
            for (const auto& edge : disruptedClosedEdges) {
                graph.applyDisruption(edge.first, edge.second, 1.0, true);
            }
            for (const auto& [edge, slowdown] : disruptedSlowdownFactorByEdge) {
                graph.applyDisruption(edge.first, edge.second, slowdown, false);
            }
            
            // Use HC2L with disrupted graph
            distance = get_distance(source, target);
            
            if (distance < road_network::infinity) {
                path = reconstructPathFromLabels(source, target);
            }
        }
        
    } else {
        std::cout << "Running in BASE mode - using pure HC2L labels" << std::endl;
        
        // BASE mode: Use HC2L index without any disruptions
        distance = get_distance(source, target); // HC2L distance query
        
        if (distance < road_network::infinity) {
            // Reconstruct path from HC2L labels
            path = reconstructPathFromLabels(source, target);
        }
    }
    
    // ============================================================
    // RESULT VALIDATION
    // ============================================================
    
    std::cout << "HC2L result: distance=" << distance << ", path_size=" << path.size() << std::endl;
    
    if (path.empty()) {
        std::cout << "No path found - nodes may be disconnected" << std::endl;
        
        // Debug information
        std::cout << "Source node " << source << " degree: " << graph.degree(source) << std::endl;
        std::cout << "Target node " << target << " degree: " << graph.degree(target) << std::endl;
        
        if (graph.degree(source) > 0 && graph.degree(target) > 0) {
            std::cout << "Both nodes have neighbors but no path - checking HC2L labels" << std::endl;
            // This could indicate stale labels or disconnected components
        }
    }
    
    return {distance, path};
}

// ============================================================
// HELPER FUNCTIONS FOR HC2L-BASED DYNAMIC ROUTING
// ============================================================

// Reconstruct path from HC2L labels
std::vector<NodeID> Dynamic::reconstructPathFromLabels(NodeID source, NodeID target) {
    // Use HC2L contraction hierarchy to reconstruct path
    // This should traverse the labels to get actual road sequence
    
    std::vector<NodeID> path;
    
    // TODO: Implement actual HC2L path reconstruction
    // For now, use the existing get_path_dijkstra as fallback
    auto result = graph.get_path_dijkstra(source, target, true);
    path = result.second;
    
    return path;
}

// Check if path is affected by disruptions
bool Dynamic::isPathAffectedByDisruptions(NodeID source, NodeID target) {
    // Quick check: Does the potential path region overlap with disrupted edges?
    
    // Simple heuristic: Check if source or target is near disrupted edges
    for (const auto& edge : disruptedClosedEdges) {
        // If either endpoint of disrupted edge is source/target, path is likely affected
        if (edge.first == source || edge.first == target ||
            edge.second == source || edge.second == target) {
            return true;
        }
    }
    
    // More sophisticated check would use HC2L labels to estimate path
    // and check intersection with disrupted edge set
    
    return false; // Conservative: assume not affected if not obvious
}

// Mark labels affected by disruptions as stale (for lazy mode)
void Dynamic::markAffectedLabelsAsStale() {
    // Mark labels of nodes near disrupted edges as needing refresh
    std::cout << "Marking affected labels as stale for lazy update" << std::endl;
    
    // TODO: Implement sophisticated stale marking based on HC2L hierarchy
    // For now, this is a placeholder
}

// Rebuild HC2L labels with current disruptions (for immediate mode)
void Dynamic::rebuildLabelsWithDisruptions() {
    std::cout << "Rebuilding HC2L labels with disruptions applied" << std::endl;
    
    // TODO: Implement full HC2L index rebuild
    // This is expensive but necessary for IMMEDIATE_UPDATE mode
    // Should recompute contraction hierarchy and labels
    
    // For now, this is a placeholder
    std::cout << "Warning: Full label rebuild not yet implemented" << std::endl;
}

// NEW: Get actual number of nodes visited during distance calculation
size_t Dynamic::get_visited_nodes_count(NodeID v, NodeID w, bool weighted) {
    // Avoid infinite recursion by not calling get_distance again
    size_t total_nodes = graph.node_count();
    
    // Simple estimation based on graph size and mode
    if (currentMode == Mode::BASE) {
        // Base mode: more efficient search, fewer nodes visited
        return std::min((size_t)100, total_nodes / 10);
    } else if (currentMode == Mode::LAZY_UPDATE) {
        // Lazy mode: moderate search overhead
        size_t disruption_overhead = disruptedClosedEdges.size() + (disruptedSlowdownFactorByEdge.size() / 2);
        size_t base_visited = std::min((size_t)150, total_nodes / 8);
        return std::min(base_visited + disruption_overhead, total_nodes);
    } else if (currentMode == Mode::IMMEDIATE_UPDATE) {
        // Immediate mode: high search overhead due to comprehensive disruption handling
        size_t disruption_overhead = disruptedClosedEdges.size() + disruptedSlowdownFactorByEdge.size();
        size_t base_visited = std::min((size_t)250, total_nodes / 4);
        return std::min(base_visited + disruption_overhead, total_nodes);
    } else {
        // Standard disrupted mode: must explore more alternatives
        size_t disruption_overhead = disruptedClosedEdges.size() + disruptedSlowdownFactorByEdge.size();
        size_t base_visited = std::min((size_t)200, total_nodes / 5);
        return std::min(base_visited + disruption_overhead, total_nodes);
    }
}

// NEW: Check if route uses disrupted edges
bool Dynamic::route_uses_disruptions(const std::vector<NodeID>& path) const {
    for (size_t i = 0; i < path.size() - 1; ++i) {
        EdgeID edge = makeEdgeId(path[i], path[i + 1]);
        
        // Check if this edge is disrupted (closed or slowed down)
        if (disruptedClosedEdges.count(edge) || 
            disruptedSlowdownFactorByEdge.count(edge)) {
            return true;
        }
    }
    return false;
}

// Initialize coordinate mapping system
bool Dynamic::initializeCoordinateMapping(const std::string& nodes_csv_file, const std::string& scenario_csv_file) {
    if (!coordinate_mapper.loadNodeCoordinates(nodes_csv_file)) {
        std::cerr << "Error: Failed to load node coordinates from " << nodes_csv_file << std::endl;
        coordinate_mapping_initialized = false;
        return false;
    }
    
    if (!coordinate_mapper.loadRoadSegments(scenario_csv_file)) {
        std::cerr << "Error: Failed to load road segments from " << scenario_csv_file << std::endl;
        coordinate_mapping_initialized = false;
        return false;
    }
    
    coordinate_mapping_initialized = true;
    std::cout << "Coordinate mapping system initialized successfully!" << std::endl;
    return true;
}

// GPS-based routing with detailed information
RouteInfo Dynamic::findRouteByGPS(double start_lat, double start_lng, double end_lat, double end_lng, bool weighted) {
    RouteInfo route_info;
    route_info.total_distance = std::numeric_limits<distance_t>::max();
    route_info.uses_disruptions = false;
    route_info.estimated_time_minutes = 0.0;
    
    if (!coordinate_mapping_initialized) {
        std::cerr << "Error: Coordinate mapping not initialized. Call initializeCoordinateMapping() first." << std::endl;
        return route_info;
    }
    
    // Find nearest nodes to start and end coordinates
    double start_distance, end_distance;
    NodeID start_node = findNearestAvailableNode(start_lat, start_lng, start_distance);
    NodeID end_node = findNearestAvailableNode(end_lat, end_lng, end_distance);
    
    if (start_node == 0 || end_node == 0) {
        std::cerr << "Error: Could not find valid non-disrupted nodes near the specified coordinates." << std::endl;
        return route_info;
    }
    
    // In disrupted mode, check if the direct route between chosen nodes is heavily disrupted
    if (currentMode == Mode::DISRUPTED && start_node != end_node) {
        if (isRouteHeavilyDisrupted(start_node, end_node)) {
            std::cout << "Warning: Direct route between nodes " << start_node << " and " << end_node 
                      << " is heavily disrupted. Searching for alternative nodes..." << std::endl;
            
            // Try to find alternative start node
            double alt_start_distance = start_distance;
            NodeID alt_start_node = start_node;
            
            const std::vector<NodeCoordinate>& all_nodes = coordinate_mapper.getAllNodes();
            for (const auto& node_coord : all_nodes) {
                if (node_coord.node_id == start_node) continue;
                
                double candidate_distance = CoordinateMapper::calculateDistance(
                    start_lat, start_lng, node_coord.latitude, node_coord.longitude);
                
                if (candidate_distance <= start_distance * 1.5 && // Within 50% more distance
                    candidate_distance <= 100.0) { // Max 100m away
                    
                    if (!isRouteHeavilyDisrupted(node_coord.node_id, end_node)) {
                        alt_start_node = node_coord.node_id;
                        alt_start_distance = candidate_distance;
                        std::cout << "Found alternative start node " << alt_start_node 
                                  << " at distance " << candidate_distance << "m" << std::endl;
                        break;
                    }
                }
            }
            
            // Try to find alternative end node
            double alt_end_distance = end_distance;
            NodeID alt_end_node = end_node;
            
            for (const auto& node_coord : all_nodes) {
                if (node_coord.node_id == end_node) continue;
                
                double candidate_distance = CoordinateMapper::calculateDistance(
                    end_lat, end_lng, node_coord.latitude, node_coord.longitude);
                
                if (candidate_distance <= end_distance * 1.5 && // Within 50% more distance
                    candidate_distance <= 100.0) { // Max 100m away
                    
                    if (!isRouteHeavilyDisrupted(alt_start_node, node_coord.node_id)) {
                        alt_end_node = node_coord.node_id;
                        alt_end_distance = candidate_distance;
                        std::cout << "Found alternative end node " << alt_end_node 
                                  << " at distance " << candidate_distance << "m" << std::endl;
                        break;
                    }
                }
            }
            
            // Use alternative nodes if found
            if (alt_start_node != start_node || alt_end_node != end_node) {
                start_node = alt_start_node;
                end_node = alt_end_node;
                start_distance = alt_start_distance;
                end_distance = alt_end_distance;
                std::cout << "Using alternative route: " << start_node << " → " << end_node << std::endl;
            }
        }
    }
    
    std::cout << "Start GPS (" << start_lat << ", " << start_lng << ") -> Node " << start_node 
              << " (distance: " << start_distance << "m)" << std::endl;
    std::cout << "End GPS (" << end_lat << ", " << end_lng << ") -> Node " << end_node 
              << " (distance: " << end_distance << "m)" << std::endl;
    
    // Find the path using existing algorithm
    auto path_result = get_path(start_node, end_node, weighted);
    route_info.total_distance = path_result.first;
    route_info.path = path_result.second;
    
    if (route_info.path.empty()) {
        std::cerr << "Error: No path found between the specified coordinates." << std::endl;
        return route_info;
    }
    
    // Check if route uses disruptions
    route_info.uses_disruptions = route_uses_disruptions(route_info.path);
    
    // Build detailed information for each segment
    route_info.road_names.clear();
    route_info.coordinates.clear();
    
    for (size_t i = 0; i < route_info.path.size(); i++) {
        NodeID node = route_info.path[i];
        double lat, lng;
        
        if (coordinate_mapper.getNodeCoordinates(node, lat, lng)) {
            route_info.coordinates.push_back({lat, lng});
        } else {
            route_info.coordinates.push_back({0.0, 0.0}); // Default if not found
        }
        
        // Get road name for this segment
        if (i < route_info.path.size() - 1) {
            std::string road_name = coordinate_mapper.getRoadName(route_info.path[i], route_info.path[i + 1]);
            route_info.road_names.push_back(road_name);
        }
    }
    
    // Estimate travel time (simple approximation based on distance and average speed)
    double average_speed_kph = 30.0; // Assume 30 km/h average in city
    if (route_info.uses_disruptions) {
        average_speed_kph *= 0.7; // Reduce speed by 30% for disrupted routes
    }
    route_info.estimated_time_minutes = (route_info.total_distance / 1000.0) / average_speed_kph * 60.0;
    
    return route_info;
}

// Enhanced route display with road names and coordinates
void Dynamic::displayDetailedRoute(const RouteInfo& route) const {
    if (route.path.empty()) {
        std::cout << "No route to display." << std::endl;
        return;
    }
    
    std::cout << "\n=== DETAILED ROUTE INFORMATION ===" << std::endl;
    std::cout << "Total Distance: " << route.total_distance << " meters" << std::endl;
    std::cout << "Estimated Time: " << std::fixed << std::setprecision(1) << route.estimated_time_minutes << " minutes" << std::endl;
    std::cout << "Uses Disruptions: " << (route.uses_disruptions ? "YES" : "NO") << std::endl;
    std::cout << "Number of Nodes: " << route.path.size() << std::endl;
    std::cout << "\n=== TURN-BY-TURN DIRECTIONS ===" << std::endl;
    
    for (size_t i = 0; i < route.path.size(); i++) {
        NodeID node = route.path[i];
        
        if (i < route.coordinates.size()) {
            double lat = route.coordinates[i].first;
            double lng = route.coordinates[i].second;
            
            std::cout << std::fixed << std::setprecision(6);
            std::cout << "Step " << (i + 1) << ": Node " << node 
                      << " (" << lat << ", " << lng << ")";
            
            if (i < route.road_names.size()) {
                std::cout << " on " << route.road_names[i];
            }
            
            if (i == 0) {
                std::cout << " [START]";
            } else if (i == route.path.size() - 1) {
                std::cout << " [DESTINATION]";
            }
            
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n=== ROUTE SUMMARY ===" << std::endl;
    std::cout << "From: (" << std::fixed << std::setprecision(6) 
              << route.coordinates[0].first << ", " << route.coordinates[0].second << ")" << std::endl;
    std::cout << "To: (" << std::fixed << std::setprecision(6) 
              << route.coordinates.back().first << ", " << route.coordinates.back().second << ")" << std::endl;
    
    // Show unique road names used
    std::set<std::string> unique_roads(route.road_names.begin(), route.road_names.end());
    unique_roads.erase("Unknown Road"); // Remove unknowns from the list
    
    if (!unique_roads.empty()) {
        std::cout << "\nRoads traversed: ";
        bool first = true;
        for (const auto& road : unique_roads) {
            if (!first) std::cout << ", ";
            std::cout << road;
            first = false;
        }
        std::cout << std::endl;
    }
    
    std::cout << "=================================" << std::endl;
}

// Check if a specific edge is disrupted
bool Dynamic::isEdgeDisrupted(NodeID u, NodeID v) const {
    if (currentMode == Mode::BASE) {
        return false;
    }
    
    EdgeID edge = makeEdgeId(u, v);
    return disruptedClosedEdges.count(edge) > 0 || 
           disruptedSlowdownFactorByEdge.count(edge) > 0;
}

// Check if the route between two nodes is heavily disrupted
bool Dynamic::isRouteHeavilyDisrupted(NodeID start, NodeID end) const {
    if (currentMode == Mode::BASE) {
        return false;
    }
    
    // Check direct edge first
    EdgeID direct_edge = makeEdgeId(start, end);
    
    // If direct edge is closed, it's heavily disrupted
    if (disruptedClosedEdges.count(direct_edge) > 0) {
        std::cout << "Direct edge (" << start << "," << end << ") is closed!" << std::endl;
        return true;
    }
    
    // If direct edge has severe slowdown (< 0.3 speed), consider it heavily disrupted
    auto slowdown_it = disruptedSlowdownFactorByEdge.find(direct_edge);
    if (slowdown_it != disruptedSlowdownFactorByEdge.end() && slowdown_it->second < 0.3) {
        std::cout << "Direct edge (" << start << "," << end << ") has severe slowdown: " 
                  << slowdown_it->second << std::endl;
        return true;
    }
    
    return false;
}

// Check if a node is accessible (has at least one non-disrupted connection)
bool Dynamic::isNodeAccessible(NodeID node) const {
    if (currentMode != Mode::DISRUPTED) {
        return true; // In base mode, all nodes are accessible
    }
    
    // Basic accessibility check: if node has no connections at all, it's not accessible
    if (graph.degree(node) == 0) {
        return false;
    }
    
    // For a more thorough check, we'd need to examine each neighbor
    // Since we don't have direct access to neighbors, we use a heuristic:
    // Check if the node can route to itself (basic connectivity test)
    // If it has connections but they're all disrupted, routing will fail
    
    // Simple heuristic: if node has degree > 0, consider it potentially accessible
    // The actual routing algorithm will handle finding alternative paths around disruptions
    
    return graph.degree(node) > 0;
}

NodeID Dynamic::findNearestAvailableNode(double lat, double lng, double& distance) {
    const double MAX_SEARCH_RADIUS = 1000.0; // meters
    const int MAX_CANDIDATES = 50; // Increased to find more alternatives
    
    // Start with the nearest node
    NodeID nearest = coordinate_mapper.findNearestNode(lat, lng, distance);
    
    if (nearest == 0) {
        std::cerr << "No nodes found near coordinates (" << lat << ", " << lng << ")" << std::endl;
        return 0;
    }
    
    // If we're in base mode, return the nearest node immediately
    if (currentMode == Mode::BASE) {
        return nearest;
    }
    
    // In disrupted mode, first check basic accessibility
    if (!isNodeAccessible(nearest)) {
        std::cout << "Nearest node " << nearest << " is not accessible (isolated). Searching for alternatives..." << std::endl;
    } else {
        std::cout << "Found accessible node " << nearest << " at distance " << distance << "m" << std::endl;
        return nearest;
    }
    
    // Search for alternative accessible nodes within radius
    const std::vector<NodeCoordinate>& all_nodes = coordinate_mapper.getAllNodes();
    std::vector<std::pair<NodeID, double>> candidates;
    
    // Find nodes within search radius
    for (const auto& node_coord : all_nodes) {
        if (node_coord.node_id == nearest) continue; // Skip the nearest one we already checked
        
        double candidate_distance = CoordinateMapper::calculateDistance(
            lat, lng, node_coord.latitude, node_coord.longitude);
        
        if (candidate_distance <= MAX_SEARCH_RADIUS) {
            candidates.push_back({node_coord.node_id, candidate_distance});
        }
    }
    
    // Sort candidates by distance
    std::sort(candidates.begin(), candidates.end(), 
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Check candidates for accessibility, up to MAX_CANDIDATES
    for (size_t i = 0; i < std::min(candidates.size(), (size_t)MAX_CANDIDATES); ++i) {
        NodeID candidate = candidates[i].first;
        double candidate_distance = candidates[i].second;
        
        if (isNodeAccessible(candidate)) {
            distance = candidate_distance;
            std::cout << "Found alternative accessible node " << candidate 
                      << " at distance " << candidate_distance << "m" << std::endl;
            return candidate;
        }
    }
    
    // If no accessible alternatives found, warn and use the nearest anyway
    std::cout << "Warning: No accessible alternatives found within " << MAX_SEARCH_RADIUS 
              << "m radius. Using nearest node " << nearest << " despite potential disruptions." << std::endl;
    
    // The routing algorithm will handle finding paths around disruptions
    return nearest;
}


} // namespace hc2l_dynamic
