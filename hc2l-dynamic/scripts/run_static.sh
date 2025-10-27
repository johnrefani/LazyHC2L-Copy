#chmod +x scripts/run_static.sh

#!/bin/bash
set -e

# Generate the Quezon City road network in .gr format
python3 sim/ingest/export_osm.py

# Build index (label construction)
build/hc2l_cli_build --in data/processed/qc.gr --out experiments/results/qc.index

# Run fixed OD queries
build/hc2l_query_od --index experiments/results/qc.index \
  --od experiments/configs/qc_od_pairs.csv \
  --out experiments/results/static_queries.csv
