# HC2L Static Testing Guide - Quezon City Dataset

This guide provides step-by-step instructions for testing the HC2L static algorithm implementation with the Quezon City dataset.

## Overview

The HC2L (Hub-based 2-Hop Labeling) static algorithm provides fast distance queries on road networks by pre-computing cut-based distance labels. This test suite validates the implementation using real-world data from Quezon City, Philippines.

## Dataset Information

- **Graph File**: `qc_from_csv.gr` (DIMACS format)
  - **Nodes**: 13,614 road network nodes
  - **Edges**: 18,784 bidirectional road segments
  - **Format**: `p sp nodes edges` header, `a source target weight` edges

- **Scenario File**: `qc_scenario_for_cpp_1.csv`
  - **Records**: 33,682 traffic condition records
  - **Node References**: 13,579 unique node IDs
  - **Fields**: source, target, road_name, speed_kph, traffic conditions, etc.

## Quick Test

### Build and Run All Tests
```bash
cd /path/to/LazyHC2L/core/hc2l_static
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd tests && ./hc2l_tests
```

### Run Only Quezon City Tests
```bash
cd build/tests
make hc2l_tests && ./hc2l_tests --gtest_filter="QuezonCityStaticTest.*"
```

## Test Descriptions

### 1. GraphLoading
**Purpose**: Validates that the QC graph loads correctly
**Expected Results**:
- 13,614 nodes loaded
- 18,784+ edges loaded
- No loading errors

### 2. GraphConnectivity  
**Purpose**: Ensures the graph is well-connected
**Expected Results**:
- Most sampled nodes have neighbors (>90%)
- Graph represents connected road network

### 3. CutIndexConstruction
**Purpose**: Tests HC2L index building performance
**Expected Results**:
- Index builds in <1 second
- Uses 10,000-20,000 shortcuts
- Index size ~10 MB
- 13,615 index entries (one per node + virtual)

### 4. DistanceQueries
**Purpose**: Validates distance query correctness
**Expected Results**:
- All random queries return finite distances (100%)
- Query time <1 microsecond average
- Same-node distance = 0

### 5. ScenarioDataValidation
**Purpose**: Analyzes CSV traffic data compatibility
**Expected Results**:
- 33,682 CSV edge records loaded
- 13,579 unique node references
- Low validity ratio (node ID mapping issue)

### 6. PerformanceBenchmark
**Purpose**: Comprehensive performance measurement
**Expected Results**:
- Index construction: <500ms
- ✅ Index size: ~10 MB
- ✅ 1000 queries: 100% reachable
- ✅ Average query time: <1 microsecond

### 7. IndexSerialization
**Purpose**: Tests index save/load functionality
**Expected Results**:
- Index serializes to binary format
- Loaded index matches original results
- All test queries return identical distances


## Performance Expectations

| Metric | Expected Value | Actual Results |
|--------|----------------|----------------|
| Graph Loading | <100ms | ~15ms |
| Index Construction | <1s | ~400ms |
| Index Size | ~10MB | 8.98-10.07MB |
| Query Time (avg) | <1μs | 0.25-0.32μs |
| Query Success Rate | 100% | 100% |
| Shortcuts Used | 10k-20k | 13k-16k |

## Traffic Data Analysis

The CSV dataset contains rich traffic information:

- **Closed Roads**: ~0.7% (241 segments)
- **Jammed Roads**: ~16.6% (jam factor > 2.0)
- **Location Types**:
  - Hospital zones: 5,680 segments
  - School zones: 5,672 segments  
  - Commercial areas: 5,669 segments
  - Residential areas: 5,540 segments
  - Arterial roads: 5,471 segments
  - Terminals: 5,650 segments


## Manual Testing Commands

### Build Index from Graph
```bash
cd build
./hc2l_cli_build --in ../test_data/qc_from_csv.gr --out qc_index.bin
```

### Query Distance Using Index
```bash
# Create simple query file (adjust node IDs as needed)
echo "source,target" > test_queries.csv
echo "0,100" >> test_queries.csv
echo "50,200" >> test_queries.csv

./hc2l_query_od --index qc_index.bin --od test_queries.csv
cat results.csv
```

## Success Criteria

All tests should pass with these characteristics:

1. **✅ Graph Loading**: Quick load without errors
2. **✅ Connectivity**: Well-connected road network  
3. **✅ Index Building**: Fast construction with reasonable size
4. **✅ Query Performance**: Sub-microsecond average query time
5. **✅ Data Compatibility**: CSV loads and parses correctly
6. **✅ Serialization**: Index persists and reloads correctly

