# DHL (Dual-Hierarchy Labeling) Developer Guide

This guide provides detailed information about where to find key components and calculations in the DHL (Dual-Hierarchy Labeling) codebase.

## Table of Contents
1. [Labeling Time Calculation](#1-labeling-time-calculation)
2. [Labeling Size Calculation](#2-labeling-size-calculation)
3. [Query Time Calculation](#3-query-time-calculation)
4. [Graph and CSV Input Handling](#4-graph-and-csv-input-handling)
5. [GPS Coordinate API](#5-gps-coordinate-api)
6. [Complete Output Structure](#6-complete-output-structure)

---

## 1. Labeling Time Calculation

### Location: `DHL/dhl_routing_service.cpp`

**Function:** `build_index()`

```cpp
// Lines 102-126
bool DHLRoutingService::build_index() {
    if (!graph) return false;
    
    auto start_time = chrono::high_resolution_clock::now();
    
    // Degree 1 node contraction
    vector<Neighbor> closest;
    graph->contract(closest);
    
    // Construct DHL index
    vector<CutIndex> ci;
    graph->create_cut_index(ci, 0.2); // 0.2 = balance parameter
    graph->reset();
    
    // Create contraction hierarchy and index
    ch = make_unique<ContractionHierarchy>();
    graph->create_contraction_hierarchy(*ch, ci, closest);
    con_index = make_unique<ContractionIndex>(ci, closest);
    
    auto end_time = chrono::high_resolution_clock::now();
    last_labeling_time_ms = chrono::duration<double, milli>(end_time - start_time).count();
    last_labeling_size_bytes = con_index->size();
    
    return true;
}
```

**Key Points:**
- Uses `chrono::high_resolution_clock` for precise timing
- Measures the complete DHL index construction process including:
  - Degree 1 node contraction
  - Cut index creation with balance parameter 0.2
  - Contraction hierarchy construction
- Result stored in `last_labeling_time_ms` (milliseconds)

### Alternative Location: `DHL/src/index.cpp`

**Standalone Index Building:**
```cpp
// Lines 10-35
int main(int argc, char** argv) {
    // read graph
    ifstream ifs(argv[1]);
    Graph g;
    read_graph(g, ifs);
    ifs.close();

    // degree 1 node contraction
    vector<Neighbor> closest;
    g.contract(closest);

    // construct index
    vector<CutIndex> ci;
    g.create_cut_index(ci, 0.2);
    g.reset();
    
    ContractionHierarchy ch;
    g.create_contraction_hierarchy(ch, ci, closest);
    ContractionIndex con_index(ci, closest);

    // write index to files
    ofstream ofs(string(argv[2]) + string("_dhl"));
    con_index.write(ofs);
    ofs.close();
}
```

**Key Points:**
- Command-line tool for precomputing DHL indices
- Saves indices to disk for later use
- Same algorithmic steps as the routing service

---

## 2. Labeling Size Calculation

### Location: `DHL/dhl_routing_service.cpp`

**Function:** `build_index()`

```cpp
// Lines 123-124
con_index = make_unique<ContractionIndex>(ci, closest);
last_labeling_size_bytes = con_index->size();
```

**Key Points:**
- Calculates memory footprint of the ContractionIndex
- Uses `con_index->size()` method which returns size in bytes
- Includes both cut index and contraction hierarchy data structures
- Result stored in `last_labeling_size_bytes`

### Size Components:
- **Cut Index**: Hierarchical labeling structure
- **Contraction Hierarchy**: Node ordering and shortcuts
- **Neighbor Lists**: Degree 1 contraction results

---

## 3. Query Time Calculation

### Location: `DHL/dhl_routing_service.cpp`

**Function:** `findRoute()`

```cpp
// Lines 247-274
// Perform DHL query
auto query_start = chrono::high_resolution_clock::now();
distance_t distance;
size_t hoplinks;

if (use_disruptions && !disrupted_edges.empty()) {
    // When disruptions are present, use Dijkstra for accurate distance calculation
    result.path = dijkstra_with_path_reconstruction(start_node, dest_node);
    // Calculate distance and hoplinks from path
} else {
    // Use precomputed DHL index when no disruptions
    distance = con_index->get_distance(start_node, dest_node);
    hoplinks = con_index->get_hoplinks(start_node, dest_node);
}

auto query_end = chrono::high_resolution_clock::now();
result.query_time_microseconds = chrono::duration<double, micro>(query_end - query_start).count();
```

**Key Points:**
- Measures actual distance query computation time
- Uses `chrono::duration<double, micro>` for microsecond precision
- Two modes:
  - **Normal**: Uses precomputed DHL index (`con_index->get_distance()`)
  - **Disrupted**: Falls back to Dijkstra algorithm for accurate results

### Location: `DHL/src/query.cpp`

**Standalone Query Tool:**
```cpp
// Lines 10-25
int main(int argc, char** argv) {
    // read index
    ifstream ifs(string(argv[1]) + string("_dhl"));
    ContractionIndex con_index(ifs);
    ifs.close();

    // read query pairs
    vector<pair<NodeID, NodeID> > queries;
    ifs.open(argv[2]);
    while(ifs >> a >> b)
        queries.push_back(make_pair(a, b));
    ifs.close();

    // run queries
    for (pair<NodeID, NodeID> q : queries)
        cout << con_index.get_distance(q.first, q.second) << endl;
}
```

**Key Points:**
- Batch query processing tool
- Loads precomputed index from disk
- Demonstrates core DHL query operation

---

## 4. Graph and CSV Input Handling

### Location: `DHL/dhl_routing_service.cpp`

**Function:** `initialize()`

```cpp
// Lines 211-257
bool DHLRoutingService::initialize(const string& graph_file, const string& coord_file, const string& disruption_file) {
    try {
        // Use provided files or detect automatically
        string final_graph_file = graph_file;
        string final_coord_file = coord_file;
        string final_disruption_file = disruption_file;
        
        // If no files provided, try to detect them
        if (graph_file.empty() || coord_file.empty()) {
            if (!find_data_files(final_graph_file, final_coord_file, final_disruption_file)) {
                cerr << "Error: Could not find required data files automatically" << endl;
                return false;
            }
        }
        
        // Load graph
        if (!load_graph(final_graph_file)) {
            return false;
        }
        current_graph_file = final_graph_file;
        
        // Load coordinates using coordinate mapper
        if (!coordinate_mapper.loadNodeCoordinates(final_coord_file)) {
            return false;
        }
        current_coord_file = final_coord_file;
        coordinate_mapping_initialized = true;
        
        // Load disruptions (optional)
        if (!final_disruption_file.empty()) {
            coordinate_mapper.loadRoadSegments(final_disruption_file);
            current_disruption_file = final_disruption_file;
        }
        
        // Build DHL index
        if (!build_index()) {
            return false;
        }
        
        return true;
    }
}
```

**Function:** `find_data_files()`

```cpp
// Lines 42-85
bool DHLRoutingService::find_data_files(string& graph_file, string& coord_file, string& disruption_file) const {
    // Possible graph file paths
    vector<string> graph_paths = {
        "data/processed/qc_from_csv.gr",
        "../data/processed/qc_from_csv.gr",
        "../../data/processed/qc_from_csv.gr",
    };
    
    // Possible coordinate file paths
    vector<string> coord_paths = {
        "data/raw/quezon_city_nodes.csv",
        "../data/raw/quezon_city_nodes.csv",
        "../../data/raw/quezon_city_nodes.csv",
    };
    
    // Find files with fallback paths
    // Returns true if both graph and coordinate files are found
}
```

**Input File Formats:**
- **Graph file** (`.gr`): Standard graph format with nodes and edges
- **Coordinate file** (`.csv`): GPS coordinates mapping `node_id,latitude,longitude`
- **Disruption file** (`.csv`): Disrupted edges/nodes data

**Key Points:**
- Supports automatic file discovery with multiple fallback paths
- Validates file existence before processing
- Initializes coordinate mapping for GPS-to-node conversion
- Builds DHL index after loading data

---

## 5. GPS Coordinate API

### Location: `DHL/dhl_routing_json_api.cpp`

**Main API Entry Point:**
```cpp
// Lines 42-65
int main(int argc, char* argv[]) {
    // Check arguments
    if (argc != 6) {
        cout << "{\"success\": false, \"error\": \"Usage: " << argv[0] 
             << " <start_lat> <start_lng> <dest_lat> <dest_lng> <use_disruptions>\"}";
        return 1;
    }
    
    try {
        // Parse arguments
        double start_lat = stod(argv[1]);
        double start_lng = stod(argv[2]);
        double dest_lat = stod(argv[3]);
        double dest_lng = stod(argv[4]);
        bool use_disruptions = (string(argv[5]) == "true" || string(argv[5]) == "1");
        
        // Initialize DHL routing service
        DHLRoutingService dhl_service;
        if (!dhl_service.initialize()) {
            cout << "{\"success\": false, \"error\": \"Failed to initialize DHL routing service\"}";
            return 1;
        }
        
        // Compute route
        auto result = dhl_service.findRoute(start_lat, start_lng, dest_lat, dest_lng, use_disruptions);
    }
}
```

### Location: `DHL/dhl_routing_service.cpp`

**Function:** `findRoute()`
```cpp
// Lines 259-320
DHLRoutingResult findRoute(double start_lat, double start_lng, 
                          double dest_lat, double dest_lng,
                          bool use_disruptions, double threshold_meters = 1000.0);
```

**GPS-to-Node Conversion:**
```cpp
// Lines 130-142
NodeID DHLRoutingService::find_nearest_node(double lat, double lng, double threshold_meters) const {
    if (!coordinate_mapping_initialized) {
        return 0;
    }
    
    double distance;
    NodeID node = coordinate_mapper.findNearestNode(lat, lng, distance);
    
    // Check if within threshold
    if (distance > threshold_meters) {
        return 0;
    }
    
    return node;
}
```

**Flask Integration Example:**
```python
# Command-line usage:
./dhl_routing_json_api 14.647631 121.064644 14.644476 121.064569 false
```

**Key Points:**
- Accepts GPS coordinates as floating-point parameters
- Supports disruption mode toggle
- Uses configurable distance threshold for node matching (default: 1000m)
- Returns comprehensive JSON response
- Integrated with coordinate mapping system

---

## 6. Complete Output Structure

### Location: `DHL/dhl_routing_json_api.cpp`

**JSON Response Structure:**
```cpp
// Lines 75-165
{
  "success": true,
  "algorithm": "DHL (Dual-Hierarchy Labelling)",
  "metrics": {
    "query_time_microseconds": 1250,
    "query_time_ms": 1250,
    "labeling_time_ms": 1520.75,
    "labeling_size_bytes": 2048576,
    "total_distance_units": 1850,
    "path_length": 15,
    "hoplinks_examined": 8,
    "routing_mode": "NORMAL",
    "uses_disruptions": false
  },
  "index_stats": {
    "index_height": 12,
    "avg_cut_size": 156.3,
    "total_labels": 50234,
    "graph_nodes": 2500,
    "graph_edges": 6780
  },
  "gps_mapping": {
    "start_node": 1247,
    "dest_node": 1891,
    "gps_to_node_info": "Start: (14.647631, 121.064644) -> Node 1247 at (14.647629, 121.064641); Dest: (14.644476, 121.064569) -> Node 1891 at (14.644478, 121.064571)"
  },
  "route": {
    "complete_trace": "DHL Route (1247 -> 1356 -> 1489 -> 1891)",
    "path_nodes": [1247, 1356, 1489, 1891],
    "coordinates": [
      {"type": "start", "lat": 14.647631, "lng": 121.064644, "node_id": 1247},
      {"type": "destination", "lat": 14.644476, "lng": 121.064569, "node_id": 1891}
    ]
  },
  "disruptions": {
    "enabled": false,
    "blocked_edges": [],
    "blocked_nodes": []
  },
  "data_sources": {
    "graph_file": "data/processed/qc_from_csv.gr",
    "coordinates_file": "data/raw/quezon_city_nodes.csv",
    "disruptions_file": "data/disruptions/qc_scenario_for_cpp_1.csv"
  },
  "input": {
    "start_lat": 14.647631,
    "start_lng": 121.064644,
    "dest_lat": 14.644476,
    "dest_lng": 121.064569,
    "use_disruptions": false
  }
}
```

**Key Output Components:**

1. **Performance Metrics:**
   - `query_time_microseconds`: Actual routing computation time
   - `labeling_time_ms`: Index construction time
   - `labeling_size_bytes`: Memory footprint of index
   - `hoplinks_examined`: Number of labels examined during query

2. **Index Statistics:**
   - `index_height`: Height of the cut hierarchy
   - `avg_cut_size`: Average size of cuts in the hierarchy
   - `total_labels`: Total number of distance labels stored

3. **Route Information:**
   - `path_nodes`: Sequence of node IDs in the shortest path
   - `complete_trace`: Human-readable route description
   - `total_distance_units`: Total path distance

4. **GPS Mapping:**
   - Node IDs corresponding to input GPS coordinates
   - Actual coordinates of matched nodes
   - Distance-based matching information

5. **Disruption Support:**
   - Lists of blocked edges and nodes
   - Disruption mode status

---

## Development Notes

### Build Instructions
```bash
cd DHL
cmake -S . -B build
cmake --build build -j
```

### Testing the API
```bash
# Example API call
./build/dhl_routing_json_api 14.647631 121.064644 14.644476 121.064569 false
```

### Key Files to Modify
- **Core algorithm**: `DHL/src/road_network.h` and `DHL/src/road_network.cpp`
- **API interface**: `DHL/dhl_routing_json_api.cpp`
- **Routing service**: `DHL/dhl_routing_service.cpp`
- **Coordinate mapping**: `DHL/dhl_coordinate_mapper.cpp`

### DHL vs D-HC2L Differences
- **DHL**: Static labeling, precomputed indices, faster queries
- **D-HC2L**: Dynamic updates, lazy/immediate modes, disruption handling
- **Query Method**: DHL uses `con_index->get_distance()`, D-HC2L uses more complex routing logic
- **Index Structure**: DHL uses pure cut hierarchy, D-HC2L adds dynamic update mechanisms

This guide provides the roadmap for understanding and modifying the DHL (Dual-Hierarchy Labeling) system, which serves as the baseline algorithm for comparison with the Dynamic HC2L implementation.