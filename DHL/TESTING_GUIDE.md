# DHL Testing Guide

## Quick Start

The recommended test to run is `test_qc_dhl` which tests the DHL algorithm on the Quezon City dataset.

### Single Command Test
```bash
make test_qc && ./test_qc_dhl
```

---

## Available Test Programs

### 1. **test_qc_dhl** (Recommended)
**Purpose**: Test DHL on Quezon City real-world dataset

**Build**:
```bash
make test_qc
```

**Run**:
```bash
./test_qc_dhl
```

**What it does**:
- Converts `../data/processed/qc_scenario_for_cpp_1.csv` to DIMACS format
- Builds DHL index on 1,000 road segments (809 intersections)
- Generates 20 sample queries across the network
- Tests shortest-path queries with performance metrics
- Outputs results and statistics

**Expected Output**:
```
Converting CSV to DIMACS format...
Graph has 809 nodes and 1000 edges
Building DHL index...
Index built in 0.0008 seconds (0.026 MB)
Generated 20 queries
Running 20 queries...
Query performance: 0.08 microseconds average
All queries completed successfully!
```

---

### 2. **test_dhl** (Comprehensive)
**Purpose**: Full DHL testing with queries and updates

**Build**:
```bash
make test_dhl
```

**Run**:
```bash
./test_dhl
```

**What it does**:
- Tests both sample data and Quezon City dataset
- Includes update operations (may cause issues with large graphs)
- More comprehensive but potentially unstable

**Note**: ⚠️ May encounter segmentation faults during update operations on large datasets

---

## Manual Testing Options

### Build Individual Components
```bash
# Build all components
make all

# Build specific components
make index    # Index construction tool
make query    # Query processing tool  
make update   # Update operations tool
```

### Manual Index/Query Process
```bash
# 1. Build index from graph file
./index quezon_city_graph.txt quezon_city_index.idx

# 2. Run queries
./query quezon_city_index.idx quezon_city_queries.txt
```

---

## Test Data Files

### Generated During Testing
- `quezon_city_graph.txt` - DIMACS format graph (809 nodes, 1000 edges)
- `quezon_city_queries.txt` - Sample queries for testing
- `qc_test_graph.txt` - Smaller test subset (500 edges)
- `qc_test_queries.txt` - Queries for smaller subset

### Original Data Source
- `../data/processed/qc_scenario_for_cpp_1.csv` - Quezon City road network

---

## Troubleshooting

### Build Issues
```bash
# Clean and rebuild
make clean
make test_qc
```

### Missing Data Files
Ensure the CSV file exists:
```bash
ls -la ../data/processed/qc_scenario_for_cpp_1.csv
```

### Permission Issues
```bash
chmod +x test_qc_dhl
```

### Large Output
Redirect output to file:
```bash
./test_qc_dhl > test_results.log 2>&1
```

---

## Performance Benchmarks

### Expected Performance (Quezon City Dataset)
- **Index Construction**: < 1ms
- **Index Size**: ~0.026 MB  
- **Query Time**: < 0.1 microseconds
- **Throughput**: > 10 million queries/second

### Hardware Requirements
- **RAM**: Minimum 1GB available
- **CPU**: Any modern processor (multi-core preferred)
- **Storage**: ~50MB for test files

---

## Testing Workflow

### 1. Quick Validation
```bash
make test_qc && ./test_qc_dhl
```

### 2. Performance Analysis
```bash
./test_qc_dhl | grep -E "(seconds|microseconds|MB)"
```

### 3. Detailed Logging
```bash
./test_qc_dhl 2>&1 | tee detailed_results.log
```

### 4. Memory Usage Check
```bash
valgrind --tool=massif ./test_qc_dhl
```

---

## Integration with Larger System

### For HC2L Dynamic Integration
The DHL implementation can be integrated into the HC2L dynamic system:

```bash
# Navigate to HC2L dynamic directory
cd ../core/hc2l_dynamic

# Build HC2L dynamic system
make

# Run HC2L with DHL preprocessing
./main_dynamic graph.txt disruptions.csv
```

### Data Pipeline
1. **CSV Data** → DHL Test → **DIMACS Graph**
2. **DIMACS Graph** → HC2L Dynamic → **Route Planning**
3. **Route Planning** → Real-time Updates → **Navigation System**

---

## File Cleanup

### Remove Test Files
```bash
make clean
```

### Remove All Generated Files
```bash
rm -f *.txt *.log *.idx test_qc_dhl test_dhl
```

### Reset to Clean State
```bash
make clean
rm -f test_qc_dhl test_dhl *.txt *.log
```

---

## Automation Scripts

### Available Scripts
- `test_qc_dataset.sh` - Automated QC testing
- `run_test.sh` - General test runner

### Run Automated Tests
```bash
chmod +x test_qc_dataset.sh
./test_qc_dataset.sh
```

---

## Success Criteria

✅ **Successful Test Run Should Show**:
- Graph conversion completes without errors
- Index builds in < 1 second  
- All queries return valid results
- No segmentation faults or crashes
- Performance metrics within expected ranges

❌ **Common Issues**:
- Missing CSV data file
- Compilation errors due to C++20 requirements
- Segmentation faults during updates (use query-only tests)
- Out of memory errors (reduce dataset size)

---

## Next Steps

After successful testing:
1. **Integration**: Incorporate into larger navigation system
2. **Optimization**: Tune parameters for specific use cases  
3. **Scaling**: Test with larger datasets
4. **Deployment**: Package for production use

For questions or issues, refer to `DATASET_EXPLANATION.md` for detailed technical information.