
#LazyHC2L 

---

## 📁 Project Structure

```
hc2l-dynamic/
├── core/
│   └── hc2l_static/       # Main C++ HC2L source code
├── data/
│   └── processed/         # Input .gr files
├── experiments/
│   ├── configs/           # OD pair CSV files
│   └── results/           # Output: results.csv, dijkstra_results.csv
├── sim/
│   └── ingest/            # Python scripts for OD and Dijkstra
├── build/                 # CMake build output
```

---

## 🧰 Prerequisites

- CMake
- MinGW or a modern GCC compiler (supports C++20)
- Python 3.10+
- Python packages: `pandas`, `networkx`

---

## ⚙️ Building the C++ Binaries

```bash
cd hc2l-dynamic

# One-time build setup
cmake -G "MinGW Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build
```

---

## 🔨 Binaries You Will Use

| Binary | Description |
|--------|-------------|
| `hc2l_cli_build.exe` | Builds the HC2L index from a `.gr` file |
| `hc2l_query_od.exe`  | Runs OD queries using the built index |

---

## 📌 Step-by-Step Usage

### 1. ✅ Prepare Input Graph

Make sure you have a valid `.gr` file inside `data/processed/`, e.g.:
```
data/processed/qc_from_csv.gr
```

---

### 2. 🔧 Build the HC2L Index

```bash
.\build\core\hc2l_static\hc2l_cli_build.exe `
  --in data\processed\qc_from_csv.gr `
  --out experiments
esults\qc.index
```

This creates:  
✔ `qc.index` – the hierarchical cut-label index file.

---

### 3. 🧮 Generate OD Pairs (Python)

```bash
python sim/ingest/generate_od_pairs.py
```

Generates:
✔ `experiments/configs/qc_od_pairs.csv`

---

### 4. 🚀 Run OD Queries with HC2L

```bash
.\build\core\hc2l_static\hc2l_query_od.exe `
  --index experiments\results\qc.index `
  --od experiments\configs\qc_od_pairs.csv
```

Outputs:
✔ `experiments/results/results.csv`  
Contains columns: `source`, `target`, `distance_meters`, `time_microseconds`, `disconnected`

---

### 5. 🧭 Baseline: Dijkstra Comparison

```bash
python sim/ingest/run_dijkstra.py
```

Outputs:
✔ `experiments/results/dijkstra_results.csv`

---

### 6. 📊 Compare HC2L vs Dijkstra

```bash
python experiments/results/compare_hc2l_vs_dijkstra.py
```

Outputs:
✔ `comparison_report.csv`  
Prints summary:
```
[SUMMARY]
Total OD pairs:       99
Disconnected pairs:    0
Matches within 1.0m:  23
Mismatches > 1.0m:    76
Match rate:         23.23%
```

---

## 📎 File Formats

### `.gr` (DIMACS-style)
```
p sp <node_count> <edge_count>
a <source> <target> <weight>
...
```

### OD Pairs CSV
```
source,target
123,456
789,101
...
```

### HC2L Results
```
source,target,distance_meters,time_microseconds,disconnected
```

---

## 🧼 Cleaning Build
```bash
rd /s /q build
```

---

## ✅ Current Capabilities

- Load road network from `.gr`
- Partition & index using HC2L
- Query OD pairs using precomputed labels
- Compare against Dijkstra
- Report errors & mismatches

---

## 🚧 Coming Soon

- Dynamic traffic weight support (HERE API or CSV)
- Shortcut updates under traffic
- Incremental index patching
- Visualization tools

---

---
