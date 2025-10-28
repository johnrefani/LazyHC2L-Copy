# Dynamic HC2L Developer Guide

This guide provides detailed information about where to find key components and calculations in the Dynamic HC2L codebase.

## Table of Contents
1. [Labeling Time Calculation](#1-labeling-time-calculation)
2. [Labeling Size Calculation](#2-labeling-size-calculation)
3. [Query Time Calculation](#3-query-time-calculation)
4. [Lazy and Immediate Update Mechanisms](#4-lazy-and-immediate-update-mechanisms)
5. [Impact Score Computation](#5-impact-score-computation)
6. [Lazy Repair Implementation](#6-lazy-repair-implementation)
7. [Graph and CSV Input Handling](#7-graph-and-csv-input-handling)
8. [GPS Coordinate API](#8-gps-coordinate-api)
9. [Complete Output Structure](#9-complete-output-structure)
10. [Disruption Detection Network](#10-disruption-detection-network)

---

## 1. Labeling Time Calculation

### Location: `core/hc2l_dynamic/gps_routing_service_impl.cpp`

**Function:** `computeLabelingMetrics()`

```cpp
// Lines 346-391
void GPSRoutingService::computeLabelingMetrics() {
    if (labeling_metrics_computed || !initialized) {
        return;  // Already computed or not initialized
    }
    
    try {
        cerr << "🔧 Computing labeling metrics (on-demand)..." << endl;
        auto labeling_start = chrono::high_resolution_clock::now();
        
        // Build ContractionIndex on the temporary graph
        vector<CutIndex> ci;
        size_t num_shortcuts = temp_graph.create_cut_index(ci, 0.5);
        
        auto labeling_end = chrono::high_resolution_clock::now();
        labeling_time_seconds = chrono::duration<double>(labeling_end - labeling_start).count();
        
        // Result stored in labeling_time_seconds
        labeling_metrics_computed = true;
    }
}
```

**Key Points:**
- Uses `chrono::high_resolution_clock` for precise timing
- Measures time to build ContractionIndex with cut parameter 0.5
- Result stored in `labeling_time_seconds` (converted to different units for display)

---

## 2. Labeling Size Calculation

### Location: `core/hc2l_dynamic/gps_routing_service_impl.cpp`

**Function:** `computeLabelingMetrics()`

```cpp
// Lines 346-391
// Create ContractionIndex to get size
ContractionIndex contraction_index(ci);
labeling_size_mb = contraction_index.size() / (1024.0 * 1024.0);
```

**Key Points:**
- Calculates memory footprint of the ContractionIndex
- Uses `contraction_index.size()` method
- Converts bytes to megabytes by dividing by 1024²

---

## 3. Query Time Calculation

### Location: `core/hc2l_dynamic/gps_routing_service_impl.cpp`

**Function:** `findRoute()`

```cpp
// Lines 267-287
// Perform GPS-based routing with real coordinate conversion
auto routing_start = chrono::high_resolution_clock::now();
auto route_info = qc_router->findRouteByGPS(start_latitude, start_longitude, dest_latitude, dest_longitude, true);
auto routing_end = chrono::high_resolution_clock::now();
auto routing_duration = chrono::duration_cast<chrono::microseconds>(routing_end - routing_start);

// Store result
result.query_time_microseconds = routing_duration.count();
```

**Key Points:**
- Measures actual routing computation time
- Uses `chrono::duration_cast<chrono::microseconds>` for precision
- Includes GPS coordinate conversion and pathfinding

---

## 4. Lazy and Immediate Update Mechanisms

### Location: `core/hc2l_dynamic/src/Dynamic.cpp`

#### Mode Detection Logic
```cpp
// Lines that determine LAZY_UPDATE vs IMMEDIATE_UPDATE
void Dynamic::loadDisruptions(const string& disruptions_file) {
    // Calculate impact scores for each disruption
    double total_impact = 0.0;
    
    // If total_impact >= tau_threshold -> IMMEDIATE_UPDATE
    // If total_impact < tau_threshold -> LAZY_UPDATE
    
    if (total_impact >= tau_threshold) {
        setMode(Mode::IMMEDIATE_UPDATE);
    } else {
        setMode(Mode::LAZY_UPDATE);
    }
}
```

#### Lazy Update Implementation
**Location:** `core/hc2l_dynamic/include/lazy_update_tracker.h`
```cpp
class LazyUpdateTracker {
    // Tracks which labels need updating
    // Delays computation until actually needed
    
public:
    void markForUpdate(NodeID node);
    bool needsUpdate(NodeID node);
    void performLazyUpdate();
};
```

#### Immediate Update Implementation
```cpp
// When mode is IMMEDIATE_UPDATE:
// - Labels are precomputed immediately when disruptions are loaded
// - Background maintenance keeps labels fresh
// - Query time is faster but uses more memory
```

**Key Points:**
- **Lazy Update**: Defers label updates until needed, saves computation
- **Immediate Update**: Precomputes updates, faster queries but more memory usage
- Mode selection based on `tau_threshold` and total impact score

---

## 5. Impact Score Computation

### Location: `core/hc2l_dynamic/src/Dynamic.cpp`

**Function:** Impact score calculation (look for disruption processing logic)

```cpp
// Pseudo-code for impact score calculation
double calculateImpactScore(const Disruption& disruption) {
    // Factors considered:
    // 1. Number of affected edges
    // 2. Centrality of affected nodes
    // 3. Alternative path availability
    // 4. Traffic flow importance
    
    double impact = 0.0;
    
    // Weight based on edge importance
    impact += disruption.affected_edges.size() * edge_weight_factor;
    
    // Centrality bonus for high-traffic areas
    impact += calculateCentralityBonus(disruption.location);
    
    // Penalty if few alternative routes exist
    impact += calculateAlternativePathPenalty(disruption.location);
    
    return impact;
}
```

**Key Points:**
- Considers multiple factors for comprehensive impact assessment
- Used to determine whether to use LAZY_UPDATE or IMMEDIATE_UPDATE
- Threshold comparison against `tau_threshold` parameter

---

## 6. Lazy Repair Implementation

### Location: `core/hc2l_dynamic/src/lazy_update_tracker.cpp`

```cpp
class LazyUpdateTracker {
    vector<bool> needs_update;
    vector<NodeID> update_queue;
    
public:
    void markForUpdate(NodeID node) {
        if (!needs_update[node]) {
            needs_update[node] = true;
            update_queue.push_back(node);
        }
    }
    
    void performLazyUpdate() {
        // Only update when query requires it
        for (NodeID node : update_queue) {
            if (needs_update[node]) {
                updateLabels(node);
                needs_update[node] = false;
            }
        }
        update_queue.clear();
    }
};
```

**Key Points:**
- Maintains queue of nodes requiring label updates
- Updates performed on-demand during queries
- Reduces computational overhead for low-impact disruptions

---

## 7. Graph and CSV Input Handling

### Location: `core/hc2l_dynamic/gps_routing_service_impl.cpp`

**Function:** `initialize()`

```cpp
// Lines 65-140
bool GPSRoutingService::initialize(const string& graph_file,
                                  const string& nodes_file,
                                  const string& disruptions_file) {
    
    // Graph file loading
    ifstream gfs(actual_graph_file);
    read_graph(graph, gfs);  // Parses .gr format
    gfs.close();
    
    // Initialize HC2L with the loaded graph
    qc_router = new Dynamic(graph);
    
    // Initialize coordinate mapping for GPS-to-node conversion
    bool mapping_success = qc_router->initializeCoordinateMapping(actual_nodes_file, actual_disruptions_file);
}
```

**Input File Formats:**
- **Graph file** (`.gr`): Standard graph format with edges and weights
- **Nodes file** (`.csv`): Node coordinates mapping `node_id,latitude,longitude`
- **Disruptions file** (`.csv`): Disruption data with affected edges/nodes

**Key Points:**
- Supports multiple file location fallbacks
- Validates file existence and readability
- Initializes coordinate mapping for GPS conversion

---

## 8. GPS Coordinate API

### Location: `core/hc2l_dynamic/gps_routing_json_api.cpp`

**Main API Entry Point:**
```cpp
int main(int argc, char* argv[]) {
    // Parse arguments: start_lat, start_lng, dest_lat, dest_lng, use_disruptions, tau_threshold
    double start_lat = stod(argv[1]);
    double start_lng = stod(argv[2]);
    double dest_lat = stod(argv[3]);
    double dest_lng = stod(argv[4]);
    bool use_disruptions = (string(argv[5]) == "true");
    double tau_threshold = stod(argv[6]);
}
```

### Location: `core/hc2l_dynamic/gps_routing_service_impl.cpp`

**Function:** `findRoute()`
```cpp
RoutingResult findRoute(double start_latitude, double start_longitude,
                       double dest_latitude, double dest_longitude,
                       bool use_disrupted_mode);
```

**Flask Integration:** `main_app/gps_hc2l_router.py`
```python
def compute_route(self, start_lat, start_lng, dest_lat, dest_lng, use_disruptions=False, tau_threshold=0.5):
    cmd = [
        self.api_executable,
        str(start_lat), str(start_lng),
        str(dest_lat), str(dest_lng),
        str(use_disruptions).lower(),
        str(tau_threshold)
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
```

**Key Points:**
- Accepts GPS coordinates as floating-point parameters
- Supports disruption mode toggle
- Configurable tau threshold for mode selection
- Returns comprehensive JSON response

---

## 9. Complete Output Structure

### Location: `core/hc2l_dynamic/gps_routing_json_api.cpp`

**JSON Response Structure:**
```cpp
// Lines 175-287
{
  "success": true,
  "algorithm": "D-HC2L (IMMEDIATE_UPDATE)",
  "algorithm_base": "D-HC2L",
  "metrics": {
    "query_time_microseconds": 1500,
    "query_time_ms": 1.5,
    "total_distance_meters": 2800.5,
    "path_length": 25,
    "routing_mode": "IMMEDIATE_UPDATE",
    "uses_disruptions": true,
    "tau_threshold": 0.5,
    "update_strategy": "immediate",
    "mode_explanation": "High-impact disruptions detected. Labels precomputed and kept fresh in background.",
    "labels_status": "precomputed_fresh",
    "labeling_time_seconds": 2.1,
    "labeling_size_mb": 15.7
  },
  "route": {
    "path_nodes": [1, 15, 23, 45, 67],
    "coordinates": [...],
    "road_segments": [...]
  },
  "disruptions": {
    "enabled": true,
    "total_impact_score": 85.2,
    "active_disruptions": 3
  }
}
```

**Key Components:**
- **Success status** and algorithm identification
- **Performance metrics** (timing, memory, path stats)
- **Mode information** (lazy/immediate, explanations)
- **Route data** (coordinates, road segments, turn directions)
- **Disruption analysis** (impact scores, active disruptions)

---

## 10. Disruption Detection Network

### Location: `core/hc2l_dynamic/src/Dynamic.cpp`

**Function:** `loadDisruptions()`
```cpp
void Dynamic::loadDisruptions(const string& disruptions_file) {
    ifstream file(disruptions_file);
    string line;
    
    while (getline(file, line)) {
        // Parse disruption data
        auto disruption = parseDisruptionLine(line);
        
        // Calculate impact score
        double impact = calculateImpactScore(disruption);
        total_impact += impact;
        
        // Apply disruption to graph
        applyDisruption(disruption);
    }
    
    // Determine mode based on total impact
    if (total_impact >= tau_threshold) {
        setMode(Mode::IMMEDIATE_UPDATE);
    } else {
        setMode(Mode::LAZY_UPDATE);
    }
}
```

### Location: `core/hc2l_dynamic/include/coordinate_mapper.h`

**GPS-to-Node Mapping:**
```cpp
class CoordinateMapper {
public:
    NodeID findNearestNode(double latitude, double longitude);
    bool isNodeAffected(NodeID node, const vector<Disruption>& disruptions);
    double calculateDistance(double lat1, double lng1, double lat2, double lng2);
};
```

**Key Points:**
- **Disruption file parsing**: CSV format with affected edges/nodes
- **Real-time impact assessment**: Calculates cumulative impact scores
- **Automatic mode selection**: Based on tau threshold comparison
- **Geographic mapping**: Converts GPS coordinates to graph nodes
- **Network analysis**: Identifies affected routes and alternative paths

---

## Development Notes

### Key Files to Modify
- **Algorithm logic**: `core/hc2l_dynamic/src/Dynamic.cpp`
- **API interface**: `core/hc2l_dynamic/gps_routing_json_api.cpp`
- **Performance metrics**: `core/hc2l_dynamic/gps_routing_service_impl.cpp`
- **Frontend integration**: `main_app/gps_hc2l_router.py`

This guide provides the roadmap for understanding and modifying the Dynamic HC2L system. Each section points to specific files and functions where the documented functionality is implemented.