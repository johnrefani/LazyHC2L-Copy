# DHL (Dual-Hierarchy Labelling) Test Implementation

This directory contains a complete test implementation for the Dual-Hierarchy Labelling (DHL) algorithm, which is an indexing technique designed for fast shortest-path queries on dynamic road networks.

## Overview

DHL is a state-of-the-art method for answering shortest-path queries efficiently while supporting dynamic updates to edge weights. It builds a hierarchical index that allows:

- **Fast Query Processing**: Sub-millisecond shortest-path distance queries
- **Dynamic Updates**: Efficient handling of edge weight changes without full recomputation
- **Space Efficiency**: Compact index representation
- **Scalability**: Suitable for large road networks

## Files Description

### Core Implementation
- `src/road_network.h` - Main header file with DHL data structures and algorithms
- `src/road_network.cpp` - Implementation of DHL algorithms
- `src/util.h` / `src/util.cpp` - Utility functions including timing

### Original DHL Tools  
- `src/index.cpp` - Build DHL index from graph file
- `src/query.cpp` - Process distance queries using built index
- `src/update.cpp` - Handle dynamic updates to the network

### Test Programs
- `test_dhl_simple.cpp` - **Comprehensive test program** (recommended)
- `test_dhl.cpp` - Extended test with CSV conversion
- `run_test.sh` - Convenience script to build and run tests

### Sample Data
- `Sample/graph.txt` - Small example graph in DIMACS format
- `Sample/queries.txt` - Example query pairs
- `Sample/updates.txt` - Example weight updates

## How to Build and Run

### Prerequisites
- C++ compiler with C++20 support (g++ recommended)
- Make build system

### Building
```bash
# Build all programs
make all

# Build specific components
make test_simple    # Recommended test program
make index         # Index builder
make query         # Query processor  
make update        # Update handler
```

### Running the Test

**Option 1: Simple Test (Recommended)**
```bash
./test_dhl_simple
```

This test program demonstrates:
1. Loading the sample graph (10 nodes, 14 edges)
2. Building the DHL index
3. Processing sample queries
4. Verifying correctness against Dijkstra's algorithm
5. Testing with Quezon City CSV data (if available)

**Option 2: Full Test with CSV Data**
```bash
./test_dhl
```

This extended test converts CSV road network data to graph format and performs comprehensive testing.

**Option 3: Using Individual Tools**
```bash
# Build index
./index Sample/graph.txt sample_index

# Process queries
./query sample_index Sample/queries.txt

# Apply updates  
./topcut Sample/graph.txt sample_index Sample/updates.txt i
```

## Test Results Explanation

### Index Construction
The test shows:
- **Contraction**: Number of degree-1 nodes removed for optimization
- **Cut Index Size**: Number of cut labels in the hierarchy
- **Shortcuts**: Additional edges added during preprocessing
- **Construction Time**: Time to build the complete index

### Query Performance
For each query, you'll see:
- **Distance**: Shortest path distance between source and target nodes
- **Query Time**: Time taken to answer the query (typically microseconds)
- **Correctness Verification**: Comparison with Dijkstra's algorithm results

### Index Statistics
- **Index Size**: Memory usage of the hierarchical index
- **Average/Max Cut Size**: Statistics about the cuts in the hierarchy
- **Height**: Depth of the hierarchical decomposition

## DHL Algorithm Details

### Index Construction Process
1. **Graph Preprocessing**: Contract degree-1 nodes to reduce graph size
2. **Hierarchical Decomposition**: Recursively partition graph using balanced cuts
3. **Label Computation**: Calculate distance labels for each hierarchy level
4. **Contraction Hierarchy**: Build CH structure for efficient updates
5. **Index Optimization**: Apply pruning and compression techniques

### Query Processing
1. **Label Lookup**: Access precomputed distance labels
2. **LCA Computation**: Find lowest common ancestor in hierarchy
3. **Distance Calculation**: Combine labels at appropriate hierarchy level
4. **Result Return**: Optimal distance in microseconds

### Dynamic Updates
1. **Impact Assessment**: Determine which labels are affected by the update
2. **Incremental Update**: Modify only affected parts of the index
3. **Consistency Maintenance**: Ensure index remains optimal

## Data Format

### Graph Format (DIMACS)
```
p sp <nodes> <edges>
a <source> <target> <weight>
...
```

### Query Format
```
<source> <target>
...
```

### Update Format
```
<source> <target> <new_weight>
...
```

## Expected Output

When running `./test_dhl_simple`, you should see:

```
DHL (Dual-Hierarchy Labelling) Test Program
===========================================

=== Testing DHL with Sample Data ===
Loading sample graph...
Sample graph loaded successfully!
Nodes: 10, Edges: 14

Building cut index...
Cut index built in 0.000028 seconds
Index construction completed!
Index size: 0.449 KB

=== Testing Distance Queries ===
Distance from 1 to 2: 12
Distance from 1 to 6: 10
Distance from 5 to 10: 4
...

=== Verifying Correctness ===
Query (1, 2): Index=12, Dijkstra=12 ✓ CORRECT
...

=== All Tests Completed ===
```

## Testing with Your Own Data

To test DHL with your own road network data:

1. **Convert to DIMACS format**: Use the CSV conversion function as a template
2. **Prepare query file**: List source-target pairs for testing
3. **Run the test**: Modify the file paths in the test program
4. **Analyze results**: Check performance and correctness metrics

## Performance Characteristics

- **Query Time**: Typically 1-10 microseconds per query
- **Index Size**: Usually 10-50% of original graph size
- **Construction Time**: Seconds to minutes depending on graph size
- **Update Time**: Milliseconds for incremental changes

## Troubleshooting

### Common Issues
1. **Segmentation Fault**: Usually indicates node ID mapping issues
2. **Large Distance Values**: May indicate disconnected graph components  
3. **Compilation Errors**: Ensure C++20 support and proper includes

### Solutions
1. **Check Node IDs**: Ensure sequential numbering starting from 1
2. **Verify Graph Connectivity**: Use smaller connected components for testing
3. **Update Compiler**: Use g++ 10.0 or later with `-std=c++2a`

## Academic Reference

This implementation is based on the Dual-Hierarchy Labelling technique for dynamic shortest-path queries on road networks. The algorithm provides theoretical guarantees for query time and space usage while maintaining practical efficiency for real-world applications.

---

**Note**: This test implementation demonstrates core DHL functionality. For production use, additional optimizations and error handling may be required.